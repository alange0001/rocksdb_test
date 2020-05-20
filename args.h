
#pragma once

#include <string>
#include <vector>
#include <functional>

struct Args {
	const char*              param_delimiter = ";";

	uint32_t                 duration = 0;
	uint32_t                 stats_interval = 0;

	uint32_t                 num_dbs = 0;
	bool                     db_create = false;
	std::vector<std::string> db_path;
	std::vector<std::string> db_config_file;
	std::vector<uint64_t>    db_num_keys;
	std::vector<uint64_t>    db_cache_size;
	std::vector<uint32_t>    db_sine_cycles;
	std::vector<uint32_t>    db_sine_shift;
	std::vector<std::string> db_bench_params;

	std::string              io_device;

	bool                     debug_output = false;
	bool                     debug_output_iostat = false;

	void parseArgs(int argc, char** argv);

private:
	std::vector<std::string> strList(const std::string& str);
	std::vector<uint64_t> uint64List(const std::string& str, std::function<void(uint64_t)> check_method);
	std::vector<uint32_t> uint32List(const std::string& str, std::function<void(uint32_t)> check_method);
};
