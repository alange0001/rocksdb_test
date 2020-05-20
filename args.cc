
#include "args.h"

#include <string>
#include <stdexcept>

#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "util.h"

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
DEFINE_uint32(stats_interval, 5,
          "Statistics interval (seconds)");

DEFINE_uint32(num_dbs, 1,
          "Number of databases");
DEFINE_bool(db_create, false,
          "Create the database");
DEFINE_string(db_path, "/media/auto/work/rocksdb",
          "Database Path");
DEFINE_string(db_config_file, "files/rocksdb.options",
          "Database Configuration File");
DEFINE_string(db_num_keys, "50000000",
          "Number of keys in the database");
DEFINE_string(db_cache_size, "268435456", // 256MiB
          "Database cache size");
DEFINE_string(db_sine_cycles, "1",
          "Number of sine cycles in the experiment");
DEFINE_string(db_sine_shift, "0",
          "Shift of sine cycle in minutes");
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

#define print_arg(name) \
		spdlog::info("Args." #name ": {}", name)
#define print_arg_list(name) \
	for (int i=0; i<name.size(); i++) \
		spdlog::info("Args." #name "[{}]: {}", i, name[i])

Args::Args(int argc, char** argv){
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

	std::string params_str(fmt::format("--log_level={}",   FLAGS_log_level));
	params_str += fmt::format(" --duration=\"{}\"",        FLAGS_duration);
	params_str += fmt::format(" --stats_interval=\"{}\"",  FLAGS_stats_interval);
	params_str += fmt::format(" --num_dbs=\"{}\"",         FLAGS_num_dbs);
	params_str += fmt::format(" --db_create=\"{}\"",       FLAGS_db_create);
	params_str += fmt::format(" --db_path=\"{}\"",         FLAGS_db_path);
	params_str += fmt::format(" --db_config_file=\"{}\"",  FLAGS_db_config_file);
	params_str += fmt::format(" --db_num_keys=\"{}\"",     FLAGS_db_num_keys);
	params_str += fmt::format(" --db_cache_size=\"{}\"",   FLAGS_db_cache_size);
	params_str += fmt::format(" --db_sine_cycles=\"{}\"",  FLAGS_db_sine_cycles);
	params_str += fmt::format(" --db_sine_shift=\"{}\"",   FLAGS_db_sine_shift);
	params_str += fmt::format(" --db_bench_params=\"{}\"", FLAGS_db_bench_params);
	params_str += fmt::format(" --io_device=\"{}\"",       FLAGS_io_device);
	spdlog::info("parameters: {}", params_str);

	debug_output           = FLAGS_debug_output;
	debug_output_iostat    = debug_output || FLAGS_debug_output_iostat;

	duration            = FLAGS_duration;
	stats_interval      = FLAGS_stats_interval;
		if (stats_interval < 1) throw std::invalid_argument("invalid --stats_interval (must be > 0)");
	num_dbs             = FLAGS_num_dbs; if (num_dbs < 1) throw std::invalid_argument("invalid --num_dbs (must be > 0)");
	db_create           = FLAGS_db_create;
	db_path             = pathList(FLAGS_db_path);
	db_config_file      = strList(FLAGS_db_config_file);
	db_num_keys         = uint64List(FLAGS_db_num_keys, [](uint64_t v){
		if (v < 1) throw std::invalid_argument("invalid --db_num_keys (must be > 0)"); });
	db_cache_size       = uint64List(FLAGS_db_cache_size, [](uint64_t v){
		if (v < (1024 * 1024)) throw std::invalid_argument("invalid --db_cache_size (must be >= 1 MiB)"); });
	db_sine_cycles      = uint32List(FLAGS_db_sine_cycles, [](uint32_t v){
		if (v < 1) throw std::invalid_argument("invalid --db_sine_cycles (must be > 0)"); });
	db_sine_shift       = uint32List(FLAGS_db_sine_shift, [](uint32_t v){});
	db_bench_params     = strList(FLAGS_db_bench_params);
	io_device           = FLAGS_io_device;

	print_arg(duration);
	print_arg(stats_interval);
	print_arg(num_dbs);
	print_arg(db_create);
	print_arg_list(db_path);
	print_arg_list(db_config_file);
	print_arg_list(db_num_keys);
	print_arg_list(db_cache_size);
	print_arg_list(db_sine_cycles);
	print_arg_list(db_sine_shift);
	print_arg_list(db_bench_params);
	print_arg(io_device);
}

std::vector<std::string> Args::strList(const std::string& str) {
	std::vector<std::string> ret = split_str(str, param_delimiter);
	if (num_dbs < ret.size())
		throw std::invalid_argument("the list size is greater than num_dbs");
	if (num_dbs == 1 && ret.size() > 1)
		throw std::invalid_argument("the list must have either one element or num_dbs");
	while (num_dbs > ret.size()) {
		ret.push_back(ret[0]);
	}
	return ret;
}

std::vector<std::string> Args::pathList(const std::string& str) {
	std::vector<std::string> ret = split_str(str, param_delimiter);
	if (num_dbs != ret.size())
		throw std::invalid_argument("number of paths in --db_path must be equal to num_dbs");
	for (int i=0; i<ret.size(); i++) {
		if (ret[i] == "") throw std::invalid_argument("empty path in --db_path");
		for (int j=i+1; j<ret.size(); j++) {
			if (ret[i] == ret[j]) throw std::invalid_argument("duplicated paths in --db_path");
		}
	}
	return ret;
}

std::vector<uint64_t> Args::uint64List(const std::string& str, std::function<void(uint64_t)> check_method) {
	std::vector<uint64_t> ret;
	auto aux = strList(str);
	for (auto i : aux) {
		uint64_t aux2 = parseUint64(i);
		check_method(aux2);
		ret.push_back(aux2);
	}
	return ret;
}

std::vector<uint32_t> Args::uint32List(const std::string& str, std::function<void(uint32_t)> check_method) {
	std::vector<uint32_t> ret;
	auto aux = strList(str);
	for (auto i : aux) {
		uint32_t aux2 = parseUint32(i);
		check_method(aux2);
		ret.push_back(aux2);
	}
	return ret;
}
