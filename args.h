
#pragma once

#include <string>
#include <vector>
#include <functional>

using std::string;
using std::vector;
using std::function;

////////////////////////////////////////////////////////////////////////////////////

#define ARG__duration__t  uint32_t
#define ARG__duration__ft DEFINE_uint32
#define ARG__duration__d  60
#define ARG__duration__fm "Duration of the experiment (min)"

#define ARG__stats_interval__t  uint32_t
#define ARG__stats_interval__ft DEFINE_uint32
#define ARG__stats_interval__d  5
#define ARG__stats_interval__fm "Statistics interval (seconds)"

#define ARG__num_dbs__t  uint32_t
#define ARG__num_dbs__ft DEFINE_uint32
#define ARG__num_dbs__d  1
#define ARG__num_dbs__fm "Number of databases"

#define ARG__db_create__t  bool
#define ARG__db_create__ft DEFINE_bool
#define ARG__db_create__d  false
#define ARG__db_create__fm "Create the database"

#define ARG__db_path__t  string
#define ARG__db_path__ft DEFINE_string
#define ARG__db_path__d  "/media/auto/work/rocksdb"
#define ARG__db_path__fm "Database Path (list)"
#define ARG__db_path__lt string
#define ARG__db_path__ln num_dbs
#define ARG__db_path__lc true
#define ARG__db_path__lf parseList_string

#define ARG__db_config_file__t  string
#define ARG__db_config_file__ft DEFINE_string
#define ARG__db_config_file__d  "files/rocksdb.options"
#define ARG__db_config_file__fm "Database Configuration File (list)"
#define ARG__db_config_file__lt string
#define ARG__db_config_file__ln num_dbs
#define ARG__db_config_file__lc true
#define ARG__db_config_file__lf parseList_string

#define ARG__db_num_keys__t  string
#define ARG__db_num_keys__ft DEFINE_string
#define ARG__db_num_keys__d  "50000000"
#define ARG__db_num_keys__fm "Number of keys in the database (list)"
#define ARG__db_num_keys__lt uint64_t
#define ARG__db_num_keys__ln num_dbs
#define ARG__db_num_keys__lc value > 1000
#define ARG__db_num_keys__lf parseList_uint64_t

#define ARG__db_cache_size__t  string
#define ARG__db_cache_size__ft DEFINE_string
#define ARG__db_cache_size__d  "268435456"
#define ARG__db_cache_size__fm "Database cache size (list)"
#define ARG__db_cache_size__lt uint64_t
#define ARG__db_cache_size__ln num_dbs
#define ARG__db_cache_size__lc value >= (1024 * 1024)
#define ARG__db_cache_size__lf parseList_uint64_t

#define ARG__db_sine_cycles__t  string
#define ARG__db_sine_cycles__ft DEFINE_string
#define ARG__db_sine_cycles__d  "1"
#define ARG__db_sine_cycles__fm "Number of sine cycles in the experiment (list)"
#define ARG__db_sine_cycles__lt uint32_t
#define ARG__db_sine_cycles__ln num_dbs
#define ARG__db_sine_cycles__lc value > 0
#define ARG__db_sine_cycles__lf parseList_uint32_t

#define ARG__db_sine_shift__t  string
#define ARG__db_sine_shift__ft DEFINE_string
#define ARG__db_sine_shift__d  "0"
#define ARG__db_sine_shift__fm "Shift of sine cycle in minutes (list)"
#define ARG__db_sine_shift__lt uint32_t
#define ARG__db_sine_shift__ln num_dbs
#define ARG__db_sine_shift__lc true
#define ARG__db_sine_shift__lf parseList_uint32_t

#define ARG__db_bench_params__t  string
#define ARG__db_bench_params__ft DEFINE_string
#define ARG__db_bench_params__d  "--sine_a=1000 --sine_d=4500"
#define ARG__db_bench_params__fm "Other parameters used in db_bench (list)"
#define ARG__db_bench_params__lt string
#define ARG__db_bench_params__ln num_dbs
#define ARG__db_bench_params__lc true
#define ARG__db_bench_params__lf parseList_string

#define ARG__num_at__t  uint32_t
#define ARG__num_at__ft DEFINE_uint32
#define ARG__num_at__d  0
#define ARG__num_at__fm "Number of access_time3 instances"

#define ARG__at_file__t  string
#define ARG__at_file__ft DEFINE_string
#define ARG__at_file__d  ""
#define ARG__at_file__fm "access_time3 --filename (list)"
#define ARG__at_file__lt string
#define ARG__at_file__ln num_at
#define ARG__at_file__lc value.length() > 0
#define ARG__at_file__lf parseList_string

#define ARG__at_block_size__t  string
#define ARG__at_block_size__ft DEFINE_string
#define ARG__at_block_size__d  "4"
#define ARG__at_block_size__fm "access_time3 --block_size (list)"
#define ARG__at_block_size__lt uint32_t
#define ARG__at_block_size__ln num_at
#define ARG__at_block_size__lc value >= 4
#define ARG__at_block_size__lf parseList_uint32_t

#define ARG__at_params__t  string
#define ARG__at_params__ft DEFINE_string
#define ARG__at_params__d  "--random_ratio=0.1 --write_ratio=0.3"
#define ARG__at_params__fm "other params for the access_time3 (list)"
#define ARG__at_params__lt string
#define ARG__at_params__ln num_at
#define ARG__at_params__lc true
#define ARG__at_params__lf parseList_string

#define ARG__at_script__t  string
#define ARG__at_script__ft DEFINE_string
#define ARG__at_script__d  ""
#define ARG__at_script__fm "access_time3 --command_script (list)"
#define ARG__at_script__lt string
#define ARG__at_script__ln num_at
#define ARG__at_script__lc true
#define ARG__at_script__lf parseList_string

#define ARG__io_device__t  string
#define ARG__io_device__ft DEFINE_string
#define ARG__io_device__d  ""
#define ARG__io_device__fm "I/O device to monitor in iostat"

#define ARG__log_level__t  string
#define ARG__log_level__ft DEFINE_string
#define ARG__log_level__d  "info"
#define ARG__log_level__fm "Log level (debug,info,warn,error,critical,off)"

#define ARG__debug_output__t  bool
#define ARG__debug_output__ft DEFINE_bool
#define ARG__debug_output__d  false
#define ARG__debug_output__fm "Debug the output of all subprocesses"

#define ARG__debug_output_iostat__t  bool
#define ARG__debug_output_iostat__ft DEFINE_bool
#define ARG__debug_output_iostat__d  false
#define ARG__debug_output_iostat__fm "Debug iostat output"

#define ALL_ARGS_NoList_F( _f ) \
	_f(duration);           \
	_f(stats_interval);     \
	_f(num_dbs);            \
	_f(db_create);          \
	_f(num_at);             \
	_f(io_device);          \
	_f(log_level);          \
	_f(debug_output);       \
	_f(debug_output_iostat)

#define ALL_ARGS_List_F( _f ) \
	_f(db_path);            \
	_f(db_config_file);     \
	_f(db_num_keys);        \
	_f(db_cache_size);      \
	_f(db_sine_cycles);     \
	_f(db_sine_shift);      \
	_f(db_bench_params);    \
	_f(at_file);            \
	_f(at_block_size);      \
	_f(at_params);          \
	_f(at_script)

#define ALL_ARGS_F( _f )     \
	ALL_ARGS_NoList_F( _f ); \
	ALL_ARGS_List_F( _f )

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

struct Args {
	static Args*  this_;
	const char*   param_delimiter = ";";

	Args(int argc, char** argv);
	~Args();

#	define declareList(name) \
		vector<ARG__##name##__lt> name##_list
	ALL_ARGS_List_F( declareList );
#	undef declareList

#	define declareGetHeader(name) \
		ARG__##name##__t name()
	ALL_ARGS_F( declareGetHeader );
#	undef declareGetHeader

private:
	vector<string> parseList_string(const string& str, const char* name, const uint32_t num, function<bool(const string& v)> check);
	vector<uint32_t> parseList_uint32_t(const string& str, const char* name, const uint32_t num, function<bool(const uint32_t v)> check);
	vector<uint64_t> parseList_uint64_t(const string& str, const char* name, const uint32_t num, function<bool(const uint64_t v)> check);
	void checkUniqueStr(const char* name, const vector<string>& src);
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""
