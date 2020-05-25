
#pragma once

#include <string>
#include <deque>
#include <atomic>

using std::string;
using std::atomic;
using std::deque;

////////////////////////////////////////////////////////////////////////////////////

/*_f(ARG_name, ARG_type, ARG_flag_type, ARG_flag_default, ARG_help, ARG_condition, ARG_set_event)*/
#define ALL_ARGS_Direct_F( _f )                                   \
	_f(log_level, string, DEFINE_string,                          \
		"info",                                                   \
		"Log level (debug,info)",         \
		true,                                                     \
		if(Args::this_ != nullptr) Args::this_->setLogLevel(value)); \
	_f(log_time_prefix, bool, DEFINE_bool,                        \
		true,                                                     \
		"print date and time in each line",                       \
		true,                                                     \
		if (!value) spdlog::set_pattern("[%l] %v"););             \
	_f(filename, string, DEFINE_string,                           \
		"",                                                       \
		"file name",                                              \
		value.length() != 0,                                      \
		nullptr);                                                 \
	_f(create_file, bool, DEFINE_bool,                            \
		true,                                                     \
		"create file",                                            \
		true,                                                     \
		nullptr);                                                 \
	_f(delete_file, bool, DEFINE_bool,                            \
		true,                                                     \
		"delete file if created",                                 \
		true,                                                     \
		nullptr);                                                 \
	_f(filesize, uint64_t, DEFINE_uint64,                         \
		0,                                                        \
		"file size (MiB)",                                        \
		value >= 10 || !FLAGS_create_file,                        \
		nullptr);                                                 \
	_f(block_size, uint64_t, DEFINE_uint64,                       \
		4,                                                        \
		"block size (KiB)",                                       \
		value >= 4,                                               \
		nullptr);                                                 \
	_f(flush_blocks, uint64_t, DEFINE_uint64,                     \
		1,                                                        \
		"blocks written before a flush (0 = no flush)",           \
		true,                                                     \
		nullptr);                                                 \
	_f(write_ratio, double, DEFINE_double,                        \
		0.0,                                                      \
		"writes/reads ratio (0-1)",                               \
		value >= 0.0 && value <= 1.0,                             \
		nullptr);                                                 \
	_f(random_ratio, double, DEFINE_double,                       \
		0.0,                                                      \
		"random ratio (0-1)",                                     \
		value >= 0.0 && value <= 1.0,                             \
		nullptr);                                                 \
	_f(sleep_interval, uint64_t, DEFINE_uint64,                   \
		0,                                                        \
		"sleep interval (ms)",                                    \
		true,                                                     \
		nullptr);                                                 \
	_f(sleep_count, uint64_t, DEFINE_uint64,                      \
		1,                                                        \
		"number of IOs before sleep",                             \
		value > 0,                                                \
		nullptr);                                                 \
	_f(stats_interval, uint32_t, DEFINE_uint32,                   \
		5,                                                        \
		"Statistics interval (seconds)",                          \
		value > 0,                                                \
		nullptr);                                                 \
	_f(wait, bool, DEFINE_bool,                                   \
		false,                                                    \
		"wait",                                                   \
		true,                                                     \
		nullptr);                                                 \
	_f(command_script, CommandScript, DEFINE_string,              \
		"",                                                       \
		"Script of commands. Syntax: \"time1:command1=value1,time2:command2=value2\"", \
		true,                                                     \
		nullptr)

#define ALL_ARGS_F( _f )     \
	ALL_ARGS_Direct_F( _f )


////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "CommandLine::"
struct CommandLine {
	uint64_t    time=0;
	string command;
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "CommandScript::"
class CommandScript : public deque<CommandLine> {
public:
	CommandScript& operator=(const string& script);
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

struct Args {
	static Args*  this_;

#	define declareArg(ARG_name, ARG_type, ...) ARG_type ARG_name
	ALL_ARGS_F( declareArg );
#	undef declareArg

	Args(int argc, char** argv);
	void setLogLevel(const string& value);
	void executeCommand(const string& command_line);
	string strStat();
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""
