
#pragma once

#include <string>

struct Args {
	uint32_t    duration;
	uint32_t    cycles;
	uint32_t    stats_interval;
	std::string db_path;
	std::string db_config_file;
	bool        db_create;
	uint64_t    db_num_keys;
	uint64_t    db_cache_size;
	std::string db_bench_params;
	std::string io_device;
	bool        debug_output;
	bool        debug_output_iostat;

	void parseArgs(int argc, char** argv);
};
