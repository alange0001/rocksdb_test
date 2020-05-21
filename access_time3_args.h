
#pragma once

#include <string>
#include <atomic>

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

struct Args {
	std::string              filename;
	uint64_t                 filesize;       //MiB
	uint64_t                 block_size;     //KiB
	std::atomic<uint64_t>    flush_blocks;   //blocks
	bool                     create_file;
	bool                     delete_file;
	std::atomic<double>      write_ratio;    //0-1
	std::atomic<double>      random_ratio;   //0-1
	std::atomic<uint64_t>    sleep_interval; //ns
	std::atomic<uint64_t>    sleep_count;
	uint32_t                 stats_interval; //seconds
	std::atomic<bool>        wait;

	Args(int argc, char** argv);
	bool parseLine(const std::string& command, const std::string& value);
};
