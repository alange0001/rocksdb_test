
#include <string>

#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <stdexcept>

#include "args.h"

//#define DEBUG
#ifdef DEBUG
#	define DEFAULT_log_level "debug"
#	define DEFAULT_debug_output false
#else
#	define DEFAULT_log_level "info"
#	define DEFAULT_debug_output false
#endif

DEFINE_uint32(duration, 60,
          "Duration of the experiment (min)");
DEFINE_uint32(cycles, 1,
          "Number of sine cycles in the experiment");
DEFINE_uint32(stats_interval, 5,
          "Statistics interval (seconds)");

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
DEFINE_string(db_bench_params, "--sine_a=1000 --sine_d=4500",
          "Other parameters used in db_bench");

DEFINE_string(io_device, "",
          "I/O device to monitor in iostat");

DEFINE_string(log_level, DEFAULT_log_level,
          "Log level (debug,info,warn,error,critical,off)");
DEFINE_bool(debug_output, DEFAULT_debug_output,
          "Debug the output of all subprocesses");
DEFINE_bool(debug_output_iostat, DEFAULT_debug_output,
          "Debug iostat output");

void Args::parseArgs(int argc, char** argv){
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

	debug_output           = FLAGS_debug_output;
	debug_output_iostat    = debug_output || FLAGS_debug_output_iostat;

	duration            = FLAGS_duration;          params_out += fmt::format(" --duration=\"{}\"",        duration);
	cycles              = FLAGS_cycles;            params_out += fmt::format(" --cycles=\"{}\"",          cycles);
	stats_interval      = FLAGS_stats_interval;    params_out += fmt::format(" --stats_interval=\"{}\"",  stats_interval);
	db_path             = FLAGS_db_path;           params_out += fmt::format(" --db_path=\"{}\"",         db_path);
	db_config_file      = FLAGS_db_config_file;    params_out += fmt::format(" --db_config_file=\"{}\"",  db_config_file);
	db_create           = FLAGS_db_create;         params_out += fmt::format(" --db_create=\"{}\"",       db_create);
	db_num_keys         = FLAGS_db_num_keys;       params_out += fmt::format(" --db_num_keys=\"{}\"",     db_num_keys);
	db_cache_size       = FLAGS_db_cache_size;     params_out += fmt::format(" --db_cache_size=\"{}\"",   db_cache_size);
	db_bench_params     = FLAGS_db_bench_params;   params_out += fmt::format(" --db_bench_params=\"{}\"", db_bench_params);
	io_device           = FLAGS_io_device;         params_out += fmt::format(" --io_device=\"{}\"",       io_device);

	spdlog::info("parameters: {}", params_out);

	if (stats_interval < 1)
		throw std::invalid_argument("invalid --stats_interval (must be > 0)");
	if (cycles < 1)
		throw std::invalid_argument("invalid --cycles (must be > 0)");
}
