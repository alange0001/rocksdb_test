
#include <string>
#include <csignal>
#include <thread>
#include <stdexcept>
#include <functional>

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <gflags/gflags.h>

#include "util.h"

////////////////////////////////////////////////////////////////////////////////////
DEFINE_string(filename, "",
          "file name");
DEFINE_uint64(file_size, 0,
          "file size (MiB)");
DEFINE_uint32(block_size, 4,
          "block size (KiB)");
DEFINE_double(write_ratio, 0,
          "writes/reads ratio (0-1)");
DEFINE_double(random_ratio, 0,
          "random ratio (0-1)");
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
	uint64_t    file_size;      //MiB
	uint32_t    block_size;     //KiB
	double      write_ratio;    //0-1
	double      random_ratio;   //0-1
	uint32_t    stats_interval; //s
	uint64_t    sleep_interval; //ms

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
		file_size       = FLAGS_file_size;
		block_size      = FLAGS_block_size;
		write_ratio     = FLAGS_write_ratio;
		random_ratio    = FLAGS_random_ratio;
		stats_interval  = FLAGS_stats_interval;
		sleep_interval  = FLAGS_sleep_interval;
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Program::"

class Program {
	static Program* this_;
	Args args;

	public: //---------------------------------------------------------------------
	Program() {
		Program::this_ = this;
		std::signal(SIGTERM, Program::signalWrapper);
		std::signal(SIGSEGV, Program::signalWrapper);
		std::signal(SIGINT,  Program::signalWrapper);
		std::signal(SIGILL,  Program::signalWrapper);
		std::signal(SIGABRT, Program::signalWrapper);
		std::signal(SIGFPE,  Program::signalWrapper);
	}
	~Program() {
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

			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		} catch (const std::exception& e) {
			spdlog::error(e.what());
			std::raise(SIGTERM);
			return 1;
		}
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
