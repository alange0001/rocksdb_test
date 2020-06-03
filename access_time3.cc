
#include <string>
#include <thread>
#include <stdexcept>
#include <functional>
#include <random>
#include <regex>
#include <limits>

#include <iostream>

#include <csignal>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "access_time3_args.h"
#include "util.h"
#include "process.h"

const uint32_t buffer_align = 512;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Worker::"

class Worker {
	Args*       args;
	int         filed = -1;

	std::thread        thread;
	std::thread        thread_flush;
	std::exception_ptr thread_exception;
	bool               stop_ = false;

	std::default_random_engine rand_eng;

	public: //---------------------------------------------------------------------
	uint64_t              blocks        = 0;
	uint64_t              blocks_read   = 0;
	uint64_t              blocks_write  = 0;

	Worker(Args* args_) : args(args_) {
		DEBUG_MSG("constructor");

		if (args->create_file)
			createFile();

		struct stat st;
		DEBUG_MSG("get file stats");
		if (stat(args->filename.c_str(), &st) == EOF)
			throw std::runtime_error("can't read file stats");
		if ((args->block_size * 1024) % st.st_blksize != 0)
			throw std::runtime_error("block size must be multiple of filesystem's block size");
		if (!args->create_file) {
			uint64_t size = st.st_size / 1024 / 1024;
			spdlog::info("File already created. Set --filesize={}", size);
			if (size < 10)
				throw std::runtime_error("invalid --filesize");
			args->filesize = size;
		}
		DEBUG_MSG("open file");
		int flags = O_RDWR;
		if (args->direct_io)
			flags = flags|O_DIRECT|O_DSYNC;
		filed = open(args->filename.c_str(), flags, 0640);
		if (filed < 0)
			throw std::runtime_error("can't open file");

		thread = std::thread( [this]{this->threadMain();} );
	}
	~Worker() {
		DEBUG_MSG("destructor");
		stop_ = true;
		if (thread.joinable())
			thread.join();
		if (thread_flush.joinable())
			thread_flush.join();
		if (filed >= 0) {
			DEBUG_MSG("close file");
			close(filed);
			if (args->create_file && args->delete_file) {
				spdlog::info("delete file {}", args->filename);
				std::remove(args->filename.c_str());
			}
		}
	}

	void threadMain() noexcept {
		struct alignas(buffer_align) buffer_t {
			char data[buffer_align];
		};

		spdlog::info("initiating worker thread");
		try {
			const uint32_t random_scale = 10000;
			std::uniform_int_distribution<uint32_t> rand_ratio(0,random_scale -1);

			uint64_t buffer_size = args->block_size * 1024; // KiB to B
			std::unique_ptr<buffer_t[]> buffer_mem(new buffer_t[buffer_size/buffer_align]);
			char* buffer = buffer_mem[0].data;
			randomize_buffer(buffer, buffer_size);

			uint64_t filesize_bytes = args->filesize * 1024 * 1024;
			uint64_t file_blocks = (args->filesize * 1024) / args->block_size;
			std::uniform_int_distribution<uint64_t> rand_block(0, file_blocks -1);

			uint64_t flush_count = 0;
			uint64_t sleep_count = 0;

			uint64_t cur_block = 0;

			while (!stop_) {
				if (args->wait)
					spdlog::info("worker thread in wait mode");
				while (!stop_ && args->wait) {
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
					if (! args->wait) {
						spdlog::info("exit wait mode");
						break;
					}
				}
				if (stop_) break;

				uint32_t write_ratio_uint = (uint32_t) (args->write_ratio * random_scale);
				uint32_t random_ratio_uint = (uint32_t) (args->random_ratio * random_scale);

				if (rand_ratio(rand_eng) < random_ratio_uint) { //random access
					cur_block = rand_block(rand_eng);
					if (lseek(filed, cur_block * buffer_size, SEEK_SET) == -1)
						throw std::runtime_error(fmt::format("seek error ({})", strerror(errno)));
				} else {                                        //sequential access
					cur_block++;
					if (cur_block >= file_blocks) {
						if (lseek(filed, 0, SEEK_SET) == -1)
							throw std::runtime_error(fmt::format("seek error ({})", strerror(errno)));
						cur_block = 0;
					}
				}

				if (rand_ratio(rand_eng) < write_ratio_uint) { //write
					if (write(filed, buffer, buffer_size) == -1)
						throw std::runtime_error(fmt::format("write error ({})", strerror(errno)));
					blocks_write++;

					if (!args->direct_io && args->flush_blocks) {
						if (++flush_count >= args->flush_blocks) {
							fsync(filed);
							flush_count = 0;
						}
					}
				} else {                                       //read
					if (read(filed, buffer, buffer_size) == -1)
						throw std::runtime_error(fmt::format("read error ({})", strerror(errno)));
					blocks_read++;
				}

				blocks++;

				if (args->sleep_interval > 0) {
					if (++sleep_count > args->sleep_count) {
						sleep_count = 0;
						std::this_thread::sleep_for(std::chrono::microseconds(args->sleep_interval));
					}
				}
			}
		} catch (std::exception &e) {
			DEBUG_MSG("exception received: {}", e.what());
			thread_exception = std::current_exception();
		}
		spdlog::info("worker thread finished");
	}
	bool isActive() {
		if (thread_exception) std::rethrow_exception(thread_exception);
		return !stop_;
	}
	void stop() noexcept {
		stop_ = true;
	}
	void wait(bool value=true) noexcept {
		args->wait = value;
	}

	private: //--------------------------------------------------------------------
	void createFile() {
		const uint32_t buffer_size = 1024 * 1024;
		alignas(buffer_align) char buffer[buffer_size];
		randomize_buffer(buffer, buffer_size);

		spdlog::info("creating file {}", args->filename);
		auto fd = open(args->filename.c_str(), O_CREAT|O_RDWR|O_DIRECT, 0640);
		if (fd < 0)
			throw std::runtime_error("can't create file");
		try {
			size_t write_ret;
			for (uint64_t i=0; i<args->filesize; i++) {
				if ((write_ret = write(fd, buffer, buffer_size)) == -1) {
					throw std::runtime_error(fmt::format("write error ({})", strerror(errno)));
				}
			}
			DEBUG_MSG("file created");
		} catch (std::exception& e) {
			close(fd);
			std::remove(args->filename.c_str());
			throw std::runtime_error(fmt::format("create file error: {}", e.what()));
		}
		close(fd);
	}

	void randomize_buffer(char* buffer, uint32_t size) {
		if (buffer == nullptr) throw std::runtime_error("invalid buffer");
		if (size == 0) throw std::runtime_error("invalid size");

		std::uniform_int_distribution<char> dist(0,255);

		char* b = buffer;
		for (uint32_t i=0; i<size; i++) {
			*b = dist(rand_eng);
			b++;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Reader::"

class Reader {
	Args* args;

	std::thread        thread;
	std::exception_ptr thread_exception;
	bool               stop_ = false;

	public: //---------------------------------------------------------------------
	Reader(Args* args_) : args(args_) {
		DEBUG_MSG("constructor");
		thread = std::thread( [this]{this->threadMain();} );
	}
	~Reader() {
		DEBUG_MSG("destructor");
		stop_ = true;
		if (thread.joinable())
			thread.join();
	}

	void threadMain() noexcept {
		try {
			DEBUG_MSG("command reader thread initiated");
			const uint32_t buffer_size = 512;
			char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

			std::string command;

			while (monitor_fgets(buffer, buffer_size -1, stdin, &stop_)) {
				for (char* c=buffer; *c != '\0'; c++)
					if (*c == '\n') *c = '\0';

				command = buffer;
				inplace_strip(command);

				if (command == "stop") {
					spdlog::info("stop command received");
					stop_ = true;
				} else {
					args->executeCommand(command);
				}
			}
			stop_ = true;
		} catch (std::exception& e) {
			DEBUG_MSG("exception received: {}", e.what());
			thread_exception = std::current_exception();
		}
		DEBUG_MSG("command reader thread finished");
	}

	bool isActive() {
		if (thread_exception) std::rethrow_exception(thread_exception);
		return !stop_;
	}
	void stop() noexcept {
		stop_ = true;
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Program::"

class Program {
	static Program* this_;
	std::unique_ptr<Args>   args;
	std::unique_ptr<Worker> worker;
	std::unique_ptr<Reader> reader;

	public: //---------------------------------------------------------------------
	Program() {
		DEBUG_MSG("constructor");
		Program::this_ = this;
		std::signal(SIGTERM, Program::signalWrapper);
		std::signal(SIGSEGV, Program::signalWrapper);
		std::signal(SIGINT,  Program::signalWrapper);
		std::signal(SIGILL,  Program::signalWrapper);
		std::signal(SIGABRT, Program::signalWrapper);
		std::signal(SIGFPE,  Program::signalWrapper);
	}
	~Program() {
		DEBUG_MSG("destructor");
		std::signal(SIGTERM, SIG_DFL);
		std::signal(SIGSEGV, SIG_DFL);
		std::signal(SIGINT,  SIG_DFL);
		std::signal(SIGILL,  SIG_DFL);
		std::signal(SIGABRT, SIG_DFL);
		std::signal(SIGFPE,  SIG_DFL);
		Program::this_ = nullptr;
	}

	int main(int argc, char** argv) noexcept {
		using namespace std::chrono;

		spdlog::info("Initializing program {}", argv[0]);
		try {
			args.reset(new Args(argc, argv));

			system_clock::time_point time_init = system_clock::now();
			system_clock::time_point time_elapsed = time_init;
			system_clock::time_point time_aux;

			uint64_t elapsed_blocks       = 0;
			uint64_t elapsed_blocks_read  = 0;
			uint64_t elapsed_blocks_write = 0;

			uint64_t elapsed_ms;
			uint64_t stats_interval_ms = args->stats_interval * 1000;

			worker.reset(new Worker(args.get()));
			reader.reset(new Reader(args.get()));

			bool stop = false;
			while (worker->isActive() && reader->isActive()) {

				auto cur_sec = duration_cast<seconds>(system_clock::now() - time_init).count();
				while (args->command_script.size() > 0 && args->command_script[0].time < cur_sec) {
					CommandLine c = args->command_script.front();
					args->command_script.pop_front();
					spdlog::info("command_script time={}, command: {}", c.time, c.command);
					if (c.command == "stop") {
						stop = true;
						reader->stop();
						worker->stop();
					} else {
						args->executeCommand(c.command);
					}
				}
				if (stop) break;

				if (args->duration > 0 && duration_cast<seconds>(system_clock::now() - time_init).count() > args->duration) {
					spdlog::info("duration time exceeded: {} seconds", args->duration);
					reader->stop();
					worker->stop();
					break;
				}

				std::this_thread::sleep_for(milliseconds(200));

				//// print statistics ////
				time_aux = system_clock::now();
				elapsed_ms = duration_cast<milliseconds>(time_aux - time_elapsed).count();
				if (elapsed_ms > stats_interval_ms) {
					uint64_t aux_blocks       = worker->blocks;
					uint64_t aux_blocks_read  = worker->blocks_read;
					uint64_t aux_blocks_write = worker->blocks_write;
					std::string aux_str = fmt::format("\"time\":\"{}\", \"total_MiB/s\":\"{}\", \"read_MiB/s\":\"{}\", \"write_MiB/s\":\"{}\"",
							duration_cast<seconds>(time_aux - time_init).count(),
							((aux_blocks       - elapsed_blocks)       * args->block_size * 1000)/(elapsed_ms * 1024),
							((aux_blocks_read  - elapsed_blocks_read)  * args->block_size * 1000)/(elapsed_ms * 1024),
							((aux_blocks_write - elapsed_blocks_write) * args->block_size * 1000)/(elapsed_ms * 1024)
							);
					spdlog::info("STATS: {}{}, {}{}", "{", aux_str, args->strStat(), "}");
					time_elapsed         = time_aux;
					elapsed_blocks       = aux_blocks;
					elapsed_blocks_read  = aux_blocks_read;
					elapsed_blocks_write = aux_blocks_write;
				}
			}

			resetAll();

		} catch (const std::exception& e) {
			spdlog::error(e.what());
			resetAll();
			spdlog::info("exit(1)");
			return 1;
		}
		spdlog::info("exit(0)");
		return 0;
	}

	private: //--------------------------------------------------------------------
	void resetAll() noexcept {
		worker.reset(nullptr);
		reader.reset(nullptr);
	}

	static void signalWrapper(int signal) noexcept {
		if (Program::this_)
			Program::this_->signalHandler(signal);
	}
	void signalHandler(int signal) noexcept {
		spdlog::warn("received signal {}", signal);

		std::signal(signal, SIG_DFL);

		resetAll();

		std::raise(signal);
	}
};
Program* Program::this_;

////////////////////////////////////////////////////////////////////////////////////

int main (int argc, char** argv) {
	Program p;
	return p.main(argc, argv);
}
