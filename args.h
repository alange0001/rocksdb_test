
#pragma once

#include <string>

struct Args {
	Args(int argc, char** argv);

	std::string db_path;
	std::string db_config_file;
	bool        db_create;
	uint64_t    db_num_keys;
	uint64_t    db_cache_size;
	uint32_t    hours;
	bool        debug_output;
};
