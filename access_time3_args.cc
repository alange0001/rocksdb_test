
#include "access_time3_args.h"

#include <stdexcept>

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
          "sleep interval (ns)");
DEFINE_uint64(sleep_count, 1,
          "number of IOs before sleep");
DEFINE_uint32(stats_interval, 5,
          "Statistics interval (seconds)");
DEFINE_bool(wait, false,
          "wait");

////////////////////////////////////////////////////////////////////////////////////
static bool validate_filename(const char* flagname, const std::string& value) {
   if (value.length() != 0)
     return true;
   throw std::invalid_argument(fmt::format("Invalid {}: {}", flagname, value));
}
DEFINE_validator(filename, &validate_filename);

static bool validate_filesize(const char* flagname, const uint64_t value) {
   if (value >= 10 || !FLAGS_create_file)
     return true;
   throw std::invalid_argument(fmt::format("Invalid {}: {}. Must be >= 10.", flagname, value));
}
DEFINE_validator(filesize, &validate_filesize);

static bool validate_block_size(const char* flagname, const uint64_t value) {
   if (value >= 4)
     return true;
   throw std::invalid_argument(fmt::format("Invalid {}: {}. Must be >= 4.", flagname, value));
}
DEFINE_validator(block_size, &validate_block_size);

static bool validate_ratio(const char* flagname, const double value) {
   if (value >= 0 && value <=1)
     return true;
   throw std::invalid_argument(fmt::format("Invalid {}: {}. The valid interval is [0..1].", flagname, value));
}
DEFINE_validator(write_ratio, &validate_ratio);
DEFINE_validator(random_ratio, &validate_ratio);

static bool validate_positive(const char* flagname, const uint32_t value) {
   if (value > 0)
     return true;
   throw std::invalid_argument(fmt::format("Invalid {}: {}. Must be > 0.", flagname, value));
}
static bool validate_positive(const char* flagname, const uint64_t value) {
   if (value > 0)
     return true;
   throw std::invalid_argument(fmt::format("Invalid {}: {}. Must be > 0.", flagname, value));
}
DEFINE_validator(sleep_count, &validate_positive);
DEFINE_validator(stats_interval, &validate_positive);


////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

Args::Args(int argc, char** argv) {
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

#	define assignValue(name) \
		name = FLAGS_##name; \
		params_out += fmt::format(" --" #name "=\"{}\"", name)

	assignValue(filename);
	assignValue(filesize);
	assignValue(block_size);
	assignValue(flush_blocks);
	assignValue(create_file);
	assignValue(delete_file);
	assignValue(write_ratio);
	assignValue(random_ratio);
	assignValue(sleep_interval);
	assignValue(sleep_count);
	assignValue(stats_interval);
	assignValue(wait);

	spdlog::info("parameters: {}", params_out);

	validate_filename("filename", FLAGS_filename);
	validate_filesize("filesize", FLAGS_filesize);
}

bool Args::parseLine(const std::string& command, const std::string& value) {
	DEBUG_MSG("command=\"{}\", value=\"{}\"", command, value);

	if (command == "help") {
		spdlog::info(
				"COMMANDS:\n"
				"    stop           - terminate\n"
				"    wait           - (true|false)\n"
				"    sleep_interval - nanoseconds\n"
				"    sleep_count    - [1..]\n"
				"    write_ratio    - [0..1]\n"
				"    random_ratio   - [0..1]\n"
				"    flush_blocks   - [0..]\n"
				);
		return true;
	}

#		define parseLineCommand(name, parser, required, default_) \
		if (command == #name) { \
				name = parser(value, required, default_, "invalid value for the command " #name); \
				spdlog::info("set {}={}", command, name); \
				return true; \
		}
#		define parseLineCommandValidate(name, parser, validator) \
		if (command == #name) { \
				auto aux = parser(value, true); \
				validator(command.c_str(), aux); \
				name = aux; \
				spdlog::info("set {}={}", command, aux); \
				return true; \
		}

	parseLineCommand(wait, parseBool, false, true);
	parseLineCommand(sleep_interval, parseUint64, true, 0);
	parseLineCommandValidate(sleep_count, parseUint64, validate_positive);
	parseLineCommandValidate(write_ratio, parseDouble, validate_ratio);
	parseLineCommandValidate(random_ratio, parseDouble, validate_ratio);
	parseLineCommand(flush_blocks, parseUint64, true, 0);

	return false;
}
