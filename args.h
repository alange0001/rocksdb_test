
#pragma once

#include <string>
#include <vector>
#include <functional>

#include "util.h"

using std::string;
using std::vector;
using std::function;

////////////////////////////////////////////////////////////////////////////////////

/*_f(ARG_name, ARG_type, ARG_flag_type, ARG_flag_default, ARG_help, ARG_condition, ARG_set_event)*/
#define ALL_ARGS_Direct_F( _f )                                   \
	_f(log_level, string, DEFINE_string,                          \
		"info",                                                   \
		"Log level (debug,info)",                                 \
		true,                                                     \
		if(Args::this_ != nullptr) Args::this_->setLogLevel(value)) \
	_f(duration, uint32_t, DEFINE_uint32,                         \
		60,                                                       \
		"Duration of the experiment (minutes)",                   \
		value >= 10,                                              \
		nullptr)                                                  \
	_f(stats_interval, uint32_t, DEFINE_uint32,                   \
		5,                                                        \
		"Statistics interval (seconds)",                          \
		value > 0,                                                \
		nullptr)                                                  \
	_f(num_dbs, uint32_t, DEFINE_uint32,                          \
		1,                                                        \
		"Number of databases",                                    \
		value > 0,                                                \
		nullptr)                                                  \
	_f(num_at, uint32_t, DEFINE_uint32,                           \
		0,                                                        \
		"Number of access_time3 instances",                       \
		true,                                                     \
		nullptr)                                                  \
	_f(db_create, bool, DEFINE_bool,                              \
		false,                                                    \
		"Create the database",                                    \
		true,                                                     \
		nullptr)                                                  \
	_f(io_device, string, DEFINE_string,                          \
		"",                                                       \
		"I/O device monitored by iostat",                         \
		value.length() > 0,                                       \
		nullptr)                                                  \
	_f(debug_output, bool, DEFINE_bool,                           \
		false,                                                    \
		"Debug the output of all subprocesses",                   \
		true,                                                     \
		nullptr)                                                  \
	_f(debug_output_iostat, bool, DEFINE_bool,                    \
		false,                                                    \
		"Debug iostat output",                                    \
		true,                                                     \
		nullptr)

/*_f(ARG_name, ARG_type, ARG_flag_type, ARG_flag_default, ARG_help, ARG_condition, ARG_set_event, ARG_item_type, ARG_item_condition, ARG_items)*/
#define ALL_ARGS_List_F( _f )                                     \
	_f(db_path, VectorParser<string>, DEFINE_string,              \
		"/media/auto/work/rocksdb",                               \
		"Database Path (list)",                                   \
		true,                                                     \
		nullptr,                                                  \
		string,                                                   \
		value.length() > 0,                                       \
		num_dbs)                                                  \
	_f(db_config_file, VectorParser<string>, DEFINE_string,       \
		"files/rocksdb.options",                                  \
		"Database Configuration File (list)",                     \
		true,                                                     \
		nullptr,                                                  \
		string,                                                   \
		value.length() > 0,                                       \
		num_dbs)                                                  \
	_f(db_num_keys, VectorParser<uint64_t>, DEFINE_string,        \
		"50000000",                                               \
		"Number of keys in the database (list)",                  \
		true,                                                     \
		nullptr,                                                  \
		uint64_t,                                                 \
		value > 1000,                                             \
		num_dbs)                                                  \
	_f(db_cache_size, VectorParser<uint64_t>, DEFINE_string,      \
		"268435456",                                              \
		"Database cache size (list)",                             \
		true,                                                     \
		nullptr,                                                  \
		uint64_t,                                                 \
		value >= (1024 * 1024),                                   \
		num_dbs)                                                  \
	_f(db_sine_cycles, VectorParser<uint32_t>, DEFINE_string,     \
		"1",                                                      \
		"Number of sine cycles in the experiment (list)",         \
		true,                                                     \
		nullptr,                                                  \
		uint32_t,                                                 \
		value > 0,                                                \
		num_dbs)                                                  \
	_f(db_sine_shift, VectorParser<uint32_t>, DEFINE_string,      \
		"0",                                                      \
		"Shift of sine cycle in minutes (list)",                  \
		true,                                                     \
		nullptr,                                                  \
		uint32_t,                                                 \
		true,                                                     \
		num_dbs)                                                  \
	_f(db_bench_params, VectorParser<string>, DEFINE_string,      \
		"--sine_a=1000 --sine_d=4500",                            \
		"Other parameters used in db_bench (list)",               \
		true,                                                     \
		nullptr,                                                  \
		string,                                                   \
		true,                                                     \
		num_dbs)                                                  \
	_f(at_file, VectorParser<string>, DEFINE_string,              \
		"",                                                       \
		"access_time3 --filename (list)",                         \
		true,                                                     \
		nullptr,                                                  \
		string,                                                   \
		value.length() > 0,                                       \
		num_at)                                                   \
	_f(at_block_size, VectorParser<uint32_t>, DEFINE_string,      \
		"4",                                                      \
		"access_time3 --block_size (list)",                       \
		true,                                                     \
		nullptr,                                                  \
		uint32_t,                                                 \
		value >= 4,                                               \
		num_at)                                                   \
	_f(at_params, VectorParser<string>, DEFINE_string,            \
		"--random_ratio=0.1 --write_ratio=0.3",                   \
		"other params for the access_time3 (list)",               \
		true,                                                     \
		nullptr,                                                  \
		string,                                                   \
		true,                                                     \
		num_at)                                                   \
	_f(at_script, VectorParser<string>, DEFINE_string,            \
		"",                                                       \
		"access_time3 --command_script (list)",                   \
		true,                                                     \
		nullptr,                                                  \
		string,                                                   \
		true,                                                     \
		num_at)

#define ALL_ARGS_F( _f )     \
	ALL_ARGS_Direct_F( _f )  \
	ALL_ARGS_List_F( _f )


////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

struct Args {
	static Args*  this_;
	const char*   param_delimiter = ";";

	Args(int argc, char** argv);
	~Args();

#	define declareArg(ARG_name, ARG_type, ...) ARG_type ARG_name;
	ALL_ARGS_F( declareArg );
#	undef declareArg

	void setLogLevel(const string& value);
private:
	void checkUniqueStr(const char* name, const vector<string>& src);
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""
