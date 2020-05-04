
#include <string>

#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <stdexcept>

#include "args.h"

#define DEBUG
#ifdef DEBUG
#	define DEFAULT_log_level "debug"
#	define DEFAULT_debug_output true
#else
#	define DEFAULT_log_level "warn"
#	define DEFAULT_debug_output false
#endif

DEFINE_string(db_path, "/mnt/work/rocksdb",
          "Database Path");
DEFINE_string(db_config_file, "files/rocksdb.options",
          "Database Configuration File");
DEFINE_bool(db_create, false,
          "Create the database");

DEFINE_uint64(db_num_keys, 50000000,
          "Number of keys in the database");
DEFINE_uint64(db_cache_size, 256 * 1024 * 1024,
          "Database cache size");

DEFINE_uint32(hours, 2,
          "Hours of experiment");

DEFINE_string(io_device, "",
          "I/O device to monitor in iostat");

DEFINE_string(log_level, DEFAULT_log_level,
          "Log level (debug,info,warn,error,critical,off)");
DEFINE_bool(debug_output, DEFAULT_debug_output,
          "Debug subprocesses' output");

Args::Args(int argc, char** argv){
	gflags::SetUsageMessage(std::string("\nUSAGE:\n\t") + std::string(argv[0]) +
				" [OPTIONS]...");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	//spdlog::info("Program Arguments: --db_path={}, --db_threads={}, --db_config_file={}, --db_memtables_budget={}, --log_level={}",
	//		FLAGS_db_path, FLAGS_db_threads, FLAGS_db_config_file, FLAGS_db_memtables_budget, FLAGS_log_level);

	if      (FLAGS_log_level == "debug"   ) spdlog::set_level(spdlog::level::debug);
	else if (FLAGS_log_level == "info"    ) spdlog::set_level(spdlog::level::info);
	else if (FLAGS_log_level == "warn"    ) spdlog::set_level(spdlog::level::warn);
	else if (FLAGS_log_level == "error"   ) spdlog::set_level(spdlog::level::err);
	else if (FLAGS_log_level == "critical") spdlog::set_level(spdlog::level::critical);
	else if (FLAGS_log_level == "off"     ) spdlog::set_level(spdlog::level::off);
	else throw std::invalid_argument(fmt::format("invalid --log_level={}", FLAGS_log_level));

	debug_output        = FLAGS_debug_output;

	db_path             = FLAGS_db_path;
	db_config_file      = FLAGS_db_config_file;
	db_create           = FLAGS_db_create;
	db_num_keys         = FLAGS_db_num_keys;
	db_cache_size       = FLAGS_db_cache_size;
	hours               = FLAGS_hours;
	io_device           = FLAGS_io_device;
}
