
#include "access_time3_args.h"

#include <stdexcept>
#include <regex>

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <gflags/gflags.h>

#include "util.h"

////////////////////////////////////////////////////////////////////////////////////
#define defaultValidatorMsg "Invalid {}: {}"
#define createValidator(name, type, condition, message)                            \
	static bool validate_##name(const char* flagname, const type value) {          \
	   if (condition)                                                              \
	     return true;                                                              \
	   throw std::invalid_argument(fmt::format(message, flagname, value));         \
	}                                                                              \
	DEFINE_validator(name, &validate_##name)
////////////////////////////////////////////////////////////////////////////////////
DEFINE_string(log_level, "info",
          "log level (debug,info,warn,error,critical,off)");

DEFINE_string(filename, "",
          "file name");
createValidator(filename, std::string&, (value.length() != 0), defaultValidatorMsg);

DEFINE_uint64(filesize, 0,
          "file size (MiB)");

DEFINE_uint64(block_size, 4,
          "block size (KiB)");
createValidator(block_size, uint64_t, (value >= 4), "Invalid {}: {}. Must be >= 4.");

DEFINE_uint64(flush_blocks, 1,
          "blocks written before a flush (0 = no flush)");

DEFINE_bool(create_file, true,
          "create file");
createValidator(filesize, uint64_t, (value >= 10 || !FLAGS_create_file), "Invalid {}: {}. Must be >= 10.");

DEFINE_bool(delete_file, true,
          "delete file if created");

DEFINE_double(write_ratio, 0,
          "writes/reads ratio (0-1)");
createValidator(write_ratio, double, (value >= 0 && value <=1), "Invalid {}: {}. The valid interval is [0..1].");

DEFINE_double(random_ratio, 0,
          "random ratio (0-1)");
createValidator(random_ratio, double, (value >= 0 && value <=1), "Invalid {}: {}. The valid interval is [0..1].");

DEFINE_uint64(sleep_interval, 0,
          "sleep interval (ns)");

DEFINE_uint64(sleep_count, 1,
          "number of IOs before sleep");
createValidator(sleep_count, uint64_t, (value > 0), "Invalid {}: {}. Must be > 0.");

DEFINE_uint32(stats_interval, 5,
          "Statistics interval (seconds)");
createValidator(stats_interval, uint32_t, (value > 0), "Invalid {}: {}. Must be > 0.");

DEFINE_bool(wait, false,
          "wait");

DEFINE_string(command_script, "",
          "Script of commands. Syntax: \"time1:command1=value1,time2:command2=value2\"");

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
#	undef assignValue

	params_out += fmt::format(" --command_script=\"{}\"", FLAGS_command_script);
	spdlog::info("parameters: {}", params_out);

	validate_filename("filename", FLAGS_filename);
	validate_filesize("filesize", FLAGS_filesize);

	parseCommandScript(FLAGS_command_script);
	if (FLAGS_log_level == "debug") {
		for (int i=0; i<commands.size(); i++) {
			spdlog::debug("command[{}]: {}:{}", i, commands[i].time, commands[i].command);
		}
	}
}

void Args::executeCommand(const std::string& command_line) {
	DEBUG_MSG("command_line: \"{}\"", command_line);

	auto aux = split_str(command_line, "=");
	std::string command(aux[0]);
	std::string value( (aux.size() < 2) ? "" : aux[1].c_str() );

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
		return;
	}

#	define parseLineCommand(name, parser, required, default_) \
		if (command == #name) { \
				name = parser(value, required, default_, "invalid value for the command " #name); \
				spdlog::info("set {}={}", command, name); \
				return; \
		}
#	define parseLineCommandValidate(name, parser, validator) \
		if (command == #name) { \
				auto aux = parser(value, true); \
				validator(command.c_str(), aux); \
				name = aux; \
				spdlog::info("set {}={}", command, aux); \
				return; \
		}
	parseLineCommand(wait, parseBool, false, true);
	parseLineCommand(sleep_interval, parseUint64, true, 0);
	parseLineCommandValidate(sleep_count, parseUint64, validate_sleep_count);
	parseLineCommandValidate(write_ratio, parseDouble, validate_write_ratio);
	parseLineCommandValidate(random_ratio, parseDouble, validate_random_ratio);
	parseLineCommand(flush_blocks, parseUint64, true, 0);
#	undef parseLineCommand
#	undef parseLineCommandValidate

	throw std::invalid_argument(fmt::format("Invalid command: {}", command));
}

void Args::parseCommandScript(const std::string& script) {
	if (script == "")
		return;

	std::cmatch cm;

	auto list = split_str(script, ",");
	for (auto i: list) {
		auto aux = split_str(i, ":");
		if (aux.size() != 2)
			throw std::invalid_argument(fmt::format("Invalid command in command_script: {}", i));

		uint64_t time;
		std::regex_search(aux[0].c_str(), cm, std::regex("([0-9]+)([sm]*)$"));
		if (cm.size() < 3)
			throw std::invalid_argument(fmt::format("Invalid time: {}", aux[0]));

		time = parseUint64(cm.str(1), true, 0, "invalid time");
		DEBUG_MSG("time_number={}, time_suffix={}, command:{}", cm.str(1), cm.str(2), aux[1]);
		if (cm.str(2) == "m")
			time *= 60;

		commands.push_back(CommandLine{time,aux[1]});
	}
}

std::string Args::strStat() {
	std::string ret;

#	define addArgStr(name) ret += fmt::format("{}\"{}\":\"{}\"", (ret.length()>0) ?", " :"", #name, name)
	addArgStr(wait);
	addArgStr(filesize);
	addArgStr(block_size);
	addArgStr(flush_blocks);
	addArgStr(write_ratio);
	addArgStr(random_ratio);
	addArgStr(sleep_interval);
	addArgStr(sleep_count);
#	undef addArgStr

	return ret;
}
