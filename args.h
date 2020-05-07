
#pragma once

#include <string>

struct Args {
	std::string db_path;
	std::string db_config_file;
	bool        db_create;
	uint64_t    db_num_keys;
	uint64_t    db_cache_size;
	uint32_t    hours;
	uint32_t    stats_interval;
	std::string io_device;
	bool        debug_output;
	bool        debug_output_db_bench;
	bool        debug_output_iostat;

	void parseArgs(int argc, char** argv);
};
