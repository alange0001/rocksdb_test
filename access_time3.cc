// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#include "version.h"

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
#include <libaio.h>

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <alutils/io.h>
#include <alutils/process.h>

#include "access_time3_args.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

const size_t aligned_buffer_size = 512;
struct alignas(aligned_buffer_size) aligned_buffer_t {
	char data[aligned_buffer_size];
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Stats::"

struct Stats {
	uint64_t blocks        = 0;
	uint64_t blocks_read   = 0;
	uint64_t blocks_write  = 0;
	uint64_t KB_read  = 0;
	uint64_t KB_write = 0;
	Stats operator- (const Stats& val) {
		Stats ret = *this;
		ret.blocks       -= val.blocks;
		ret.blocks_read  -= val.blocks_read;
		ret.blocks_write -= val.blocks_write;
		ret.KB_read  -= val.KB_read;
		ret.KB_write -= val.KB_write;
		return ret;
	}
	void increment(const Stats& val) {
		blocks       += val.blocks;
		blocks_read  += val.blocks_read;
		blocks_write += val.blocks_write;
		KB_read  += val.KB_read;
		KB_write += val.KB_write;
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "AIOController"

class AIORequest {
	public:  // ------------------------------------------------------------
	int              pos    = -1;
	bool             active = false;
	bool             write  = false;
	int              fd     = 0;
	io_context_t*    ctx    = nullptr;
	iocb             cb;
	Stats            stats;
	size_t           size   = 0;
	long long        offset = 0;
	void*            buffer = nullptr;

	void init(int pos_, io_context_t* ctx_, int fd_) {
		pos = pos_; ctx = ctx_; fd = fd_;
	}

	void request(bool write_, void* buffer_, size_t size_, long long offset_) {
		buffer = buffer_;
		size = size_;
		offset = offset_;
		write = write_;
		assert(pos >= 0);
		assert(!active);
		assert(size > 0);
		assert(offset >= 0);

		if (write) {
			reset_stats(0, size / 1024);
			io_prep_pwrite(&cb, fd, buffer, size, offset);
		} else { //read
			reset_stats(size / 1024, 0);
			io_prep_pread(&cb, fd, buffer, size, offset);
		}
		cb.data = this;

		iocb* iocbs[1] = {&cb};
		auto ret = io_submit(*ctx, 1, iocbs);
		if (ret == 1) {
			active = true;
		} else if (ret == 0) {
			spdlog::warn("aio submit returned 0", E2S(ret));
		} else if (ret == -EINTR || ret == -EAGAIN) {
			spdlog::warn("aio submit returned -{}", E2S(ret));
		} else {
			throw std::runtime_error(fmt::format("failed to submit aio request: -{}", E2S(ret)).c_str());
		}
	}

	bool overlap(size_t size_, long long offset_) {
		if (!active || size == 0 || size_ == 0)
			return false;
		long long end  = offset  + size  -1;
		long long end_ = offset_ + size_ -1;
		return (offset_ >= offset && offset_ <= end)
		    || (end_ >= offset && end_ <= end)
		    || (offset_ <= offset && end_ >= end);
	}

	private: // ------------------------------------------------------------

	void reset_stats(typeof(stats.KB_read) read, typeof(stats.KB_write) write) {
		stats.blocks = 1;
		stats.blocks_read  = (read  > 0) ? 1 : 0;
		stats.blocks_write = (write > 0) ? 1 : 0;
		stats.KB_read = read;
		stats.KB_write = write;
	}
};

class AIOController {
	int      fd;
	uint32_t iodepth;
	uint32_t in_progress   = 0;
	bool*    stop;
	Stats*   stats;
	io_context_t ctx;
	std::unique_ptr<AIORequest[]>  request_list;

	public:  // ------------------------------------------------------------

	AIOController(int fd_, uint32_t iodepth_, bool* stop_, Stats* stats_) :
		fd(fd_), iodepth(iodepth_),
		stop(stop_), stats(stats_)
	{
		DEBUG_MSG("constructor");
		assert(iodepth > 0 && iodepth <= max_iodepth);

		memset(&ctx, 0, sizeof(ctx));

		auto ret = io_setup(max_iodepth, &ctx);
		if (ret != 0) {
			throw std::runtime_error(fmt::format("io_setup returned error {}:{}", ret, E2S(ret)).c_str());
		}

		request_list.reset(new AIORequest[max_iodepth]);
		for (uint32_t i = 0; i < max_iodepth; i++) {
			request_list[i].init(i, &ctx, fd);
		}
	}

	~AIOController() {
		DEBUG_MSG("destructor");
		if (io_destroy(ctx) < 0) {
			spdlog::error("io_destroy returned error");
		}
	}

	bool wait_ready() {
		if (stop != nullptr && *stop)
			return false;
		if (in_progress < iodepth)
			return true;

		io_event events[max_iodepth];
		while (1) {
			if (stop != nullptr && *stop)
				return false;

			auto nevents = io_getevents(ctx, 0, iodepth, events, nullptr);
			if (stop != nullptr && *stop)
				return false;
			if (nevents == 0) {
				continue;
			} else if (nevents < 0) {
				if (nevents != -EAGAIN && nevents != -EINTR) {
					spdlog::warn("io_getevents returned {}:{}", nevents, E2S(nevents));
				} else {
					throw std::runtime_error(fmt::format("io_getevents returned error: {}:{}", nevents, E2S(nevents)).c_str());
				}
			}
			Stats stats_sum;
			uint32_t nevents_under_iodepth = 0;
			for (int i = 0; i < nevents; i++) {
				if (events[i].data) {
					in_progress--;
					auto req = ((AIORequest*) events[i].data);
					assert(req->pos >= 0 && req->pos < max_iodepth);
					assert(req->active);
					req->active = false;
					stats_sum.increment(req->stats);
					if (req->pos < iodepth) {
						nevents_under_iodepth++;
					}
				}
			}
			if (stats != nullptr) {
				stats->increment(stats_sum);
			}
			if (nevents > 0 && nevents_under_iodepth > 0)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		return true;
	}

	bool overlap(size_t size, long long offset) {
		for (int i = 0; i < max_iodepth; i++) {
			if (request_list[i].overlap(size, offset)) {
				return true;
			}
		}
		return false;
	}

	int get_free_request_number() {
		if (wait_ready()) {
			for (int i = 0; i < iodepth; i++) {
				if (!request_list[i].active) {
					return i;
				}
			}
		}
		return -1;
	}

	void request_read(int number, void* buffer, size_t size, long long offset) {
		assert(number >= 0 && number < iodepth);
		request_list[number].request(false, buffer, size, offset);
		if (request_list[number].active) {
			in_progress++;
		}
	}

	void request_write(int number, void* buffer, size_t size, long long offset) {
		assert(number >= 0 && number < iodepth);
		request_list[number].request(true, buffer, size, offset);
		if (request_list[number].active) {
			in_progress++;
		}
	}

	void update_iodepth(uint32_t new_iodepth) {
		DEBUG_MSG("iodepth changed from {} to {}", iodepth, new_iodepth);
		iodepth = new_iodepth;
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Worker::"

class Worker {
	Args*       args;
	int         filed = -1;

	std::thread        thread;
	std::exception_ptr thread_exception;
	bool               stop_ = false;

	std::unique_ptr<AIOController> iocontroller;

	std::default_random_engine rand_eng;

	public: //---------------------------------------------------------------------
	Stats stats;

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
			flags = flags|O_DIRECT;
		filed = open(args->filename.c_str(), flags, 0640);
		if (filed < 0)
			throw std::runtime_error("can't open file");

		iocontroller.reset(new AIOController(filed, args->iodepth, &stop_, &stats));

		thread = std::thread( [this]{this->threadMain();} );
	}

	~Worker() {
		DEBUG_MSG("destructor");
		stop_ = true;
		if (thread.joinable())
			thread.join();
		iocontroller.reset(nullptr);
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
		spdlog::info("initiating worker thread");
		try {
			uint64_t flush_count = 0;
			uint64_t sleep_count = 0;
			const uint32_t random_scale = 10000;

			std::uniform_int_distribution<uint32_t> rand_ratio(0,random_scale -1);
			std::unique_ptr<std::uniform_int_distribution<uint64_t>> rand_block;

			typeof(Args::block_size) cur_block_size  = 0;
			uint64_t buffer_size = 0;
			uint64_t file_blocks = 0;
			uint64_t cur_block   = 0;
			uint32_t cur_iodepth = args->iodepth;

			std::unique_ptr<aligned_buffer_t[]> buffer_list[max_iodepth];
			uint64_t buffer_size_list[max_iodepth];
			memset(buffer_size_list, 0, sizeof(uint64_t) * max_iodepth);

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

				if (cur_block_size != args->block_size) { // check block size
					DEBUG_MSG("cur_block_size changed from {} to {}", cur_block_size, args->block_size);
					cur_block_size = args->block_size;
					buffer_size = cur_block_size * 1024; // KiB to B;
					file_blocks = (args->filesize * 1024) / cur_block_size;

					rand_block.reset(new std::uniform_int_distribution<uint64_t>(0, file_blocks -1));

					cur_block = file_blocks; // seek 0 if next sequential I/O
				}

				uint32_t write_ratio_uint = (uint32_t) (args->write_ratio * random_scale);
				uint32_t random_ratio_uint = (uint32_t) (args->random_ratio * random_scale);

				long long offset;
				do {
					if (rand_ratio(rand_eng) < random_ratio_uint) { //random access
						cur_block = (*rand_block)(rand_eng);
					} else {                                        //sequential access
						cur_block++;
						if (cur_block >= file_blocks) {
							cur_block = 0;
						}
					}
					offset = cur_block * buffer_size;
				} while(iocontroller->overlap(buffer_size, offset));

				if (cur_iodepth != args->iodepth) {
					cur_iodepth = args->iodepth;
					iocontroller->update_iodepth(cur_iodepth);
				}

				auto n = iocontroller->get_free_request_number();
				if (stop_) break;

				if (n >= 0) {
					assert( n < max_iodepth );
					char* buffer;

					// check for buffer_size update in buffer_size_list
					if (buffer_size != buffer_size_list[n]) {
						DEBUG_MSG("buffer_size_list[{}] changed from {} to {}", n, buffer_size_list[n], buffer_size);
						buffer_list[n].reset(new aligned_buffer_t[buffer_size/sizeof(aligned_buffer_t)]);
						buffer = buffer_list[n][0].data;
						randomize_buffer(buffer, buffer_size);
						buffer_size_list[n] = buffer_size;
					} else {
						buffer = buffer_list[n][0].data;
					}

					// request read/write
					if (rand_ratio(rand_eng) < write_ratio_uint) { //write
						iocontroller->request_write(n, buffer, buffer_size, offset);

						if (args->flush_blocks) {
							if (++flush_count >= args->flush_blocks) {
								fdatasync(filed);
								flush_count = 0;
							}
						}
					} else {                                       //read
						iocontroller->request_read(n, buffer, buffer_size, offset);
					}
				} else {
					spdlog::warn("Invalid free request number ({}). Try again in the next loop.", n);
				}

				if (!stop_ && args->sleep_interval > 0) {
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
		alignas(aligned_buffer_size) char buffer[buffer_size];
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

			while (alutils::monitor_fgets(buffer, buffer_size -1, stdin, &stop_)) {
				for (char* c=buffer; *c != '\0'; c++)
					if (*c == '\n') *c = '\0';

				command = buffer;
				alutils::inplace_strip(command);

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
		std::signal(SIGINT,  Program::signalWrapper);
	}
	~Program() {
		DEBUG_MSG("destructor");
		std::signal(SIGTERM, SIG_DFL);
		std::signal(SIGINT,  SIG_DFL);
		Program::this_ = nullptr;
	}

	int main(int argc, char** argv) noexcept {
		using namespace std::chrono;

		spdlog::info("Initializing program access_time3 version {}", ROCKSDB_TEST_VERSION);
		try {
			args.reset(new Args(argc, argv));

			system_clock::time_point time_init = system_clock::now();
			system_clock::time_point time_elapsed = time_init;
			system_clock::time_point time_aux;

			Stats    elapsed_stats;
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
					auto cur_stats = worker->stats;
					if (! args->changed) {
						std::string aux_args = args->strStat();
						auto delta = cur_stats - elapsed_stats;
						std::string aux_str =
							fmt::format("\"time\":\"{}\"", duration_cast<seconds>(time_aux - time_init).count()) +
							fmt::format(", \"total_MiB/s\":\"{:.1f}\"", static_cast<double>((delta.KB_read + delta.KB_write) * 1000)/static_cast<double>(elapsed_ms * 1024) ) +
							fmt::format(", \"read_MiB/s\":\"{:.1f}\"",  static_cast<double>(delta.KB_read  * 1000)/static_cast<double>(elapsed_ms * 1024) ) +
							fmt::format(", \"write_MiB/s\":\"{:.1f}\"", static_cast<double>(delta.KB_write * 1000)/static_cast<double>(elapsed_ms * 1024) ) +
							fmt::format(", \"blocks/s\":\"{:.1f}\"",    static_cast<double>(delta.blocks   * 1000)/static_cast<double>(elapsed_ms) ) +
							fmt::format(", \"blocks_read/s\":\"{:.1f}\"",  static_cast<double>(delta.blocks_read  * 1000)/static_cast<double>(elapsed_ms) ) +
							fmt::format(", \"blocks_write/s\":\"{:.1f}\"", static_cast<double>(delta.blocks_write * 1000)/static_cast<double>(elapsed_ms) ) ;
						spdlog::info("STATS: {{{}, {}}}", aux_str, aux_args);
					} else { // args changed. skip stats for one period
						args->changed = false;
					}
					time_elapsed         = time_aux;
					elapsed_stats        = cur_stats;
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
