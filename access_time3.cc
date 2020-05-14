
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

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <gflags/gflags.h>

#include "util.h"

////////////////////////////////////////////////////////////////////////////////////
DEFINE_string(log_level, "info",
          "log level (debug,info,warn,error,critical,off)");
DEFINE_string(filename, "",
          "file name");
DEFINE_uint64(filesize, 0,
          "file size (MiB)");
DEFINE_uint64(block_size, 4,
          "block size (KiB)");
DEFINE_uint64(flush_blocks, 1,
          "blocks written before a flush (0 = no flush)");
DEFINE_bool(create_file, true,
          "create file");
DEFINE_bool(delete_file, true,
          "delete file if created");
DEFINE_double(write_ratio, 0,
          "writes/reads ratio (0-1)");
DEFINE_double(random_ratio, 0,
          "random ratio (0-1)");
DEFINE_uint64(sleep_interval, 0,
          "sleep interval (ms)");
DEFINE_uint32(stats_interval, 5,
          "Statistics interval (seconds)");
DEFINE_bool(wait, false,
          "wait");

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

struct Args {
	std::string filename;
	uint64_t    filesize;       //MiB
	uint64_t    block_size;     //KiB
	std::atomic<uint64_t>    flush_blocks;   //blocks
	bool        create_file;
	bool        delete_file;
	std::atomic<double>      write_ratio;    //0-1
	std::atomic<double>      random_ratio;   //0-1
	std::atomic<uint64_t>    sleep_interval; //ms
	uint32_t    stats_interval; //seconds
	std::atomic<bool>        wait;

	std::function<bool(double)> check_ratio = [](double v)->bool{return (v>=0.0 && v <= 1.0);};
	const char* error_write_ratio    = "invalid write_ratio [0-1]";
	const char* error_random_ratio   = "invalid random_ratio [0-1]";
	const char* error_sleep_interval = "invalid sleep_interval";
	const char* error_flush_blocks   = "invalid flush_blocks";

	void parseArgs(int argc, char** argv) {
		gflags::SetUsageMessage(std::string("\nUSAGE:\n\t") + std::string(argv[0]) +
					" [OPTIONS]...");
		gflags::ParseCommandLineFlags(&argc, &argv, true);

		if      (FLAGS_log_level == "debug"   ) spdlog::set_level(spdlog::level::debug);
		else if (FLAGS_log_level == "info"    ) spdlog::set_level(spdlog::level::info);
		else if (FLAGS_log_level == "warn"    ) spdlog::set_level(spdlog::level::warn);
		else if (FLAGS_log_level == "error"   ) spdlog::set_level(spdlog::level::err);
		else if (FLAGS_log_level == "critical") spdlog::set_level(spdlog::level::critical);
		else if (FLAGS_log_level == "off"     ) spdlog::set_level(spdlog::level::off);
		else throw std::invalid_argument(fmt::format("invalid --log_level={}", FLAGS_log_level));

		std::string params_out(fmt::format("--log_level={}", FLAGS_log_level));

		filename        = FLAGS_filename;         params_out += fmt::format(" --filename=\"{}\"",   filename);
		filesize        = FLAGS_filesize;         params_out += fmt::format(" --filesize={}",       filesize);
		block_size      = FLAGS_block_size;       params_out += fmt::format(" --block_size={}",     block_size);
		flush_blocks    = FLAGS_flush_blocks;     params_out += fmt::format(" --flush_blocks={}",   flush_blocks);
		create_file     = FLAGS_create_file;      params_out += fmt::format(" --create_file={}",    create_file);
		delete_file     = FLAGS_delete_file;      params_out += fmt::format(" --delete_file={}",    delete_file);
		write_ratio     = FLAGS_write_ratio;      params_out += fmt::format(" --write_ratio={}",    write_ratio);
		random_ratio    = FLAGS_random_ratio;     params_out += fmt::format(" --random_ratio={}",   random_ratio);
		sleep_interval  = FLAGS_sleep_interval;   params_out += fmt::format(" --sleep_interval={}", sleep_interval);
		stats_interval  = FLAGS_stats_interval;   params_out += fmt::format(" --stats_interval={}", stats_interval);
		wait            = FLAGS_wait;             params_out += fmt::format(" --wait={}",           wait);

		spdlog::info("parameters: {}", params_out);

		if (filename.length() == 0)
			throw std::invalid_argument("--filename not specified");
		if (filesize < 10 && create_file)
			throw std::invalid_argument("invalid --filesize");
		if (!check_ratio(write_ratio))
			throw std::invalid_argument(error_write_ratio);
		if (!check_ratio(random_ratio))
			throw std::invalid_argument(error_random_ratio);
		if (block_size < 4)
			throw std::invalid_argument("invalid --block_size");
		if (stats_interval < 1)
			throw std::invalid_argument("invalid --stats_interval (> 0)");
	}

	bool parseLine(const std::string command, const std::string value) {
		DEBUG_MSG("command=\"{}\", value=\"{}\"", command, value);

		if (command == "help") {
			spdlog::info(
					"COMMANDS:\n"
					"    stop           - terminate\n"
					"    wait           - (true|false)\n"
					"    sleep_interval - milliseconds\n"
					"    write_ratio    - (0..1)\n"
					"    random_ratio   - (0..1)\n"
					"    flush_blocks   - (0..)\n"
					);
			return true;
		} else if (command == "wait") {
			wait = parseBool(value, false, true, "invalid wait value (yes|no)");
			spdlog::info("set wait={}", wait);
			return true;
		} else if (command == "sleep_interval") {
			sleep_interval = parseUint(value, true, 0, error_sleep_interval);
			spdlog::info("set sleep_interval={}", sleep_interval);
			return true;
		} else if (command == "write_ratio") {
			write_ratio = parseDouble(value, true, 0, error_write_ratio, check_ratio);
			spdlog::info("set write_ratio={}", write_ratio);
			return true;
		} else if (command == "random_ratio") {
			random_ratio = parseDouble(value, true, 0, error_random_ratio, check_ratio);
			spdlog::info("set random_ratio={}", random_ratio);
			return true;
		} else if (command == "flush_blocks") {
			flush_blocks = parseUint(value, true, 0, error_flush_blocks);
			spdlog::info("set flush_blocks={}", flush_blocks);
			return true;
		}

		return false;
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Worker::"

class Worker {
	Args*       args;
	std::FILE*  file;

	std::thread        thread;
	std::thread        thread_flush;
	std::exception_ptr thread_exception;
	bool               stop_;

	std::atomic<bool>  flush;

	std::default_random_engine rand_eng;

	public: //---------------------------------------------------------------------
	std::atomic<uint64_t> blocks;

	Worker(Args& args_) : args(&args_) {
		DEBUG_MSG("constructor");
		stop_ = false;
		blocks = 0;
		flush = false;

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
		file = std::fopen(args->filename.c_str(), "r+");
		if (file == NULL)
			throw std::runtime_error("can't open file");

		thread       = std::thread( [this]{this->threadMain();} );
		thread_flush = std::thread( [this]{this->threadFlush();} );
	}
	~Worker() {
		DEBUG_MSG("destructor");
		stop_ = true;
		if (thread.joinable())
			thread.join();
		if (thread_flush.joinable())
			thread_flush.join();
		if (file != NULL) {
			DEBUG_MSG("close file");
			std::fclose(file);
			if (args->create_file && args->delete_file) {
				DEBUG_MSG("delete file");
				std::remove(args->filename.c_str());
			}
		}
	}

	void threadMain() noexcept {
		spdlog::info("initiating worker thread");
		try {
			const uint32_t random_scale = 10000;
			std::uniform_int_distribution<uint32_t> rand_ratio(0,random_scale -1);

			uint64_t buffer_size = args->block_size * 1024; // KiB to B
			std::unique_ptr<char[]> buffer(new char[buffer_size]);
			randomize_buffer(buffer.get(), buffer_size);

			uint64_t filesize_bytes = args->filesize * 1024 * 1024;
			uint64_t file_blocks = (args->filesize * 1024) / args->block_size;
			std::uniform_int_distribution<uint64_t> rand_block(0, file_blocks -1);

			uint64_t flush_count = 0;

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
					if (std::fseek(file, rand_block(rand_eng) * buffer_size, SEEK_SET) != 0)
						throw std::runtime_error("fseek error");
				} else {                                        //sequential access
					uint64_t pos = std::ftell(file);
					if (pos >  (filesize_bytes - buffer_size -1))
						if (std::fseek(file, 0, SEEK_SET) != 0)
							throw std::runtime_error("fseek begin error");
				}

				if (rand_ratio(rand_eng) < write_ratio_uint) { //write
					if (std::fwrite(buffer.get(), buffer_size, 1, file) != 1)
						throw std::runtime_error("fwrite error");

					if (args->flush_blocks) {
						if (++flush_count >= args->flush_blocks) {
							flush = true;
							flush_count = 0;
						}
					}
				} else {                                       //read
					if (std::fread(buffer.get(), buffer_size, 1, file) != 1)
						throw std::runtime_error("fread error");
				}

				blocks++;

				if (args->sleep_interval > 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(args->sleep_interval));
			}
		} catch (std::exception &e) {
			DEBUG_MSG("exception received: {}", e.what());
			thread_exception = std::current_exception();
		}
		spdlog::info("worker thread finished");
	}
	void threadFlush() {
		uint32_t count_wait = 0;
		while (!stop_) {
			if (file != NULL && !args->wait && flush) {
				flush = false;
				std::fflush(file);
				count_wait = 0;
				continue;
			}
			if (count_wait < 5) {
				count_wait++;
				std::this_thread::yield();
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}
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
		uint32_t buffer_size = 1024 * 1024;
		char buffer[buffer_size];
		randomize_buffer(buffer, buffer_size);

		spdlog::info("creating file {}", args->filename);
		std::FILE* f = std::fopen(args->filename.c_str(), "w");
		if (f == NULL)
			throw std::runtime_error("can't create file");
		try {
			for (uint64_t i=0; i<args->filesize; i++) {
				if (std::fwrite(buffer, buffer_size, 1, f) != 1) {
					std::fclose(f);
					std::remove(args->filename.c_str());
					throw std::runtime_error("write error");
				}
				if (i==0) {
					std::fflush(f);
					struct stat st;
					if (stat(args->filename.c_str(), &st) == EOF)
						throw std::runtime_error("can't read file stats");
					if ((args->block_size * 1024) % st.st_blksize != 0)
						throw std::runtime_error("block size must be multiple of filesystem's block size");
				}
			}
			DEBUG_MSG("file created");
		} catch (std::exception& e) {
			std::fclose(f);
			std::remove(args->filename.c_str());
			throw e;
		}
		std::fflush(f);
		std::fclose(f);
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
	Reader(Args& args_) : args(&args_) {
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

			std::cmatch cm;
			std::string line;
			std::string command;
			std::string value;

			while (monitor_fgets(buffer, buffer_size -1, stdin, &stop_)) {
				for (char* c=buffer; *c != '\0'; c++)
					if (*c == '\n') *c = '\0';

				line = buffer;
				inplace_trim(line);
				command = ""; value = "";

				std::regex_search(line.c_str(), cm, std::regex("\\s*([^=]+).*"));
				if (cm.size() >= 2)
					command = cm.str(1);
				std::regex_search(line.c_str(), cm, std::regex("[^=]+=\\s*(.*)"));
				if (cm.size() >= 2)
					value = cm.str(1);

				inplace_trim(command);
				inplace_trim(value);

				if (command == "stop") {
					spdlog::info("stop command received");
					stop_ = true;
				} else if (!args->parseLine(command, value)) {
					throw std::runtime_error(fmt::format("invalid command: {}", line));
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
	Args args;

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
		spdlog::info("Initializing program {}", argv[0]);
		try {
			args.parseArgs(argc, argv);
			std::chrono::system_clock::time_point init_time = std::chrono::system_clock::now();
			std::chrono::system_clock::time_point aux_time;
			std::chrono::system_clock::time_point elapsed_time = init_time;
			uint64_t elapsed_blocks = 0;
			uint64_t aux_blocks = 0;

			uint64_t interval_ms;
			uint64_t stats_interval_ms = args.stats_interval * 1000;

			worker.reset(new Worker(args));
			reader.reset(new Reader(args));

			while (worker->isActive() && reader->isActive()) {

				std::this_thread::sleep_for(std::chrono::milliseconds(200));

				//// print statistics ////
				aux_time = std::chrono::system_clock::now();
				interval_ms = std::chrono::duration_cast<std::chrono::milliseconds>(aux_time - elapsed_time).count();
				if (interval_ms > stats_interval_ms) {
					aux_blocks = worker->blocks;
					spdlog::info("time={}, throughput={}MiB/s{}",
							std::chrono::duration_cast<std::chrono::seconds>(aux_time - init_time).count(),
							((aux_blocks - elapsed_blocks) * args.block_size)/(interval_ms/1000)/1024,
							(args.wait) ? " (wait)" : ""
							);
					elapsed_time = aux_time;
					elapsed_blocks = aux_blocks;
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
