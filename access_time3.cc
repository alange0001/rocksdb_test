
#include <string>
#include <thread>
#include <stdexcept>
#include <functional>
#include <random>

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
DEFINE_string(filename, "",
          "file name");
DEFINE_bool(create_file, true,
          "create file");
DEFINE_uint64(filesize, 0,
          "file size (MiB)");
DEFINE_uint32(block_size, 4,
          "block size (KiB)");
DEFINE_double(write_ratio, 0,
          "writes/reads ratio (0-1)");
DEFINE_double(random_ratio, 0,
          "random ratio (0-1)");
DEFINE_bool(wait, false,
          "wait");
DEFINE_uint64(sleep_interval, 0,
          "sleep interval (ms)");
DEFINE_uint32(stats_interval, 5,
          "Statistics interval (seconds)");
DEFINE_string(log_level, "info",
          "log level (debug,info,warn,error,critical,off)");

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

struct Args {
	std::string filename;
	bool        create_file;
	uint64_t    filesize;       //MiB
	uint32_t    block_size;     //KiB
	double      write_ratio;    //0-1
	double      random_ratio;   //0-1
	bool        wait;
	uint64_t    sleep_interval; //ms
	uint32_t    stats_interval; //s

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

		filename        = FLAGS_filename;
		create_file     = FLAGS_create_file;
		filesize        = FLAGS_filesize;
		block_size      = FLAGS_block_size;
		write_ratio     = FLAGS_write_ratio;
		random_ratio    = FLAGS_random_ratio;
		wait            = FLAGS_wait;
		sleep_interval  = FLAGS_sleep_interval;
		stats_interval  = FLAGS_stats_interval;

		if (filename.length() == 0)
			throw std::invalid_argument(fmt::format("--filename not specified"));
		if (filesize < 10 && create_file)
			throw std::invalid_argument(fmt::format("invalid --filesize"));
		if (write_ratio < 0 || write_ratio > 1)
			throw std::invalid_argument(fmt::format("invalid --write_ratio"));
		if (random_ratio < 0 || random_ratio > 1)
			throw std::invalid_argument(fmt::format("invalid --random_ratio"));
		if (block_size < 4)
			throw std::invalid_argument(fmt::format("invalid --block_size"));
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Worker::"

class Worker {
	Args*       args;
	std::FILE*  file;

	std::thread        thread;
	std::exception_ptr thread_exception;
	bool               stop_ = false;

	public: //---------------------------------------------------------------------
	Worker(Args& args_) : args(&args_) {
		DEBUG_MSG("constructor");
		if (args->create_file)
			createFile();

		struct stat st;
		DEBUG_MSG("get file stats");
		if (stat(args->filename.c_str(), &st) == EOF)
			throw Exception("can't read file stats");
		if ((args->block_size * 1024) % st.st_blksize != 0)
			throw Exception("block size must be multiple of filesystem's block size");
		if (!args->create_file) {
			uint64_t size = st.st_size / 1024 / 1024;
			spdlog::info("File already created. Set --filesize={}", size);
			if (size < 10)
				throw Exception("invalid --filesize");
			args->filesize = size;
		}
		DEBUG_MSG("open file");
		file = std::fopen(args->filename.c_str(), "r+");
		if (file == NULL)
			throw Exception("can't open file");
	}
	~Worker() {
		DEBUG_MSG("destructor");
		stop_ = true;
		if (thread.joinable())
			thread.join();
		if (file != NULL) {
			DEBUG_MSG("close file");
			std::fclose(file);
			if (args->create_file) {
				DEBUG_MSG("delete file");
				std::remove(args->filename.c_str());
			}
		}
	}

	void launchThread() {
		thread = std::thread( [this]{this->threadMain();} );
	}
	void threadMain() noexcept {
		DEBUG_MSG("initiating work thread");
		try {
			while (!stop_) {
				while (!stop_ && args->wait) {
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
				if (stop_) break;

				throw Exception("test");
				//TODO: implementar acesso ao arquivo aqui
			}
		} catch (std::exception &e) {
			thread_exception = std::current_exception();
		}
		DEBUG_MSG("work thread finished");
	}
	bool isActive() {
		if (thread_exception)
			std::rethrow_exception(thread_exception);
		return !stop_;
	}
	void stop() {
		stop_ = true;
	}
	void wait(bool value=true) {
		args->wait = value;
	}

	private: //--------------------------------------------------------------------
	void createFile() {
		uint32_t buffer_size = 1024 * 1024;
		char buffer[buffer_size];
		random(buffer, buffer_size);

		DEBUG_MSG("creating file {}", args->filename);
		std::FILE* f = std::fopen(args->filename.c_str(), "w");
		if (f == NULL)
			throw Exception("can't create file");
		try {
			for (uint32_t i=0; i<args->filesize; i++) {
				if (std::fwrite(buffer, buffer_size, 1, f) != 1) {
					std::fclose(f);
					std::remove(args->filename.c_str());
					throw Exception("write error");
				}
				if (i==0) {
					std::fflush(f);
					struct stat st;
					if (stat(args->filename.c_str(), &st) == EOF)
						throw Exception("can't read file stats");
					if ((args->block_size * 1024) % st.st_blksize != 0)
						throw Exception("block size must be multiple of filesystem's block size");
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

	void random(char* buffer, uint32_t size) {
		if (buffer == nullptr) throw Exception("invalid buffer");
		if (size == 0) throw Exception("invalid size");

		std::default_random_engine gen;
		std::uniform_int_distribution<char> dist(0,255);

		char* b = buffer;
		for (uint32_t i=0; i<size; i++) {
			*b = dist(gen);
			b++;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Program::"

class Program {
	static Program* this_;
	Args args;
	std::unique_ptr<Worker> worker;

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
			worker.reset(new Worker(args));
			worker->launchThread();

			while (worker->isActive()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}

		} catch (const std::exception& e) {
			spdlog::error(e.what());
			worker.reset(nullptr);
			spdlog::info("exit(1)");
			return 1;
		}
		worker.reset(nullptr);
		spdlog::info("exit(0)");
		return 0;
	}

	private: //--------------------------------------------------------------------
	static void signalWrapper(int signal) {
		if (Program::this_)
			Program::this_->signalHandler(signal);
	}
	void signalHandler(int signal) {
		spdlog::warn("received signal {}", signal);
		if (worker.get() != nullptr)
			worker.reset(nullptr);
		std::signal(signal, SIG_DFL);
		std::raise(signal);
	}
};
Program* Program::this_;

////////////////////////////////////////////////////////////////////////////////////

int main (int argc, char** argv) {
	Program p;
	return p.main(argc, argv);
}
