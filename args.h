
#pragma once

#include <string>

class Args {
	public:
		Args(int argc, char** argv);

		std::string db_path;
		int32_t     db_threads;
		std::string db_config_file;
		uint64_t    db_memtables_budget;
};
