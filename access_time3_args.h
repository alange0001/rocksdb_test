
#pragma once

#include <string>
#include <deque>
#include <atomic>

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "CommandLine::"
struct CommandLine {
	uint64_t    time=0;
	std::string command;
};

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

	std::deque<CommandLine> commands;

	Args(int argc, char** argv);
	void executeCommand(const std::string& command_line);
	void parseCommandScript(const std::string& script);
	std::string strStat();
};
