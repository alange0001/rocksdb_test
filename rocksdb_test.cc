
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <regex>

#include <chrono>

#include <stdexcept>

#include <csignal>
#include <sys/stat.h>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "args.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "DBBenchStats::"
class DBBenchStats {
	public:
	std::map<std::string, std::string> data;

	DBBenchStats() {}
	DBBenchStats(const DBBenchStats& src) {*this = src;}
	DBBenchStats& operator= (const DBBenchStats& src) {data = src.data; return *this;}

	DBBenchStats(const char* buffer, Args* args) {
		auto flags = std::regex_constants::match_any;
		std::cmatch cm;

		std::regex_search(buffer, cm, std::regex("thread [0-9]+: \\(([0-9.]+),([0-9.]+)\\) ops and \\(([0-9.]+),([0-9.]+)\\) ops/second in \\(([0-9.]+),([0-9.]+)\\) seconds.*"), flags);
		if( cm.size() >= 7 ){
			data["ops"] = cm.str(1);
			data["ops_total"] = cm.str(2);
			data["ops_per_s"] = cm.str(3);
			data["ops_per_s_total"] = cm.str(4);
			data["stats_interval"] = cm.str(5);
			data["stats_total"] = cm.str(5);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);
		}
		std::regex_search(buffer, cm, std::regex("Interval writes: ([0-9.]+[KMGT]*) writes, ([0-9.]+[KMGT]*) keys, ([0-9.]+[KMGT]*) commit groups, ([0-9.]+[KMGT]*) writes per commit group, ingest: ([0-9.]+) [KMGT]*B, ([0-9.]+) [KMGT]*B/s.*"), flags);
		if( cm.size() >= 7 ){
			data["writes"] = cm.str(1);
			data["written_keys"] = cm.str(2);
			data["written_commit_groups"] = cm.str(3);
			data["ingest_MBps"] = cm.str(5);
			data["ingest_MBps"] = cm.str(6);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);
		}
		std::regex_search(buffer, cm, std::regex("Interval WAL: ([0-9.]+[KMGT]*) writes, ([0-9.]+[KMGT]*) syncs, ([0-9.]+[KMGT]*) writes per sync, written: ([0-9.]+) [KMGT]*B, ([0-9.]+) [KMGT]*B/s.*"), flags);
		if( cm.size() >= 5 ){
			data["WAL_writes"] = cm.str(1);
			data["WAL_syncs"] = cm.str(2);
			data["WAL_written_MB"] = cm.str(4);
			data["WAL_written_MBps"] = cm.str(5);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);
		}
		std::regex_search(buffer, cm, std::regex("Interval stall: ([0-9:.]+) H:M:S, ([0-9.]+) percent.*"), flags);
		if( cm.size() >= 3 ){
			data["stall"] = cm.str(1);
			data["stall_percent"] = cm.str(2);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);
		}
	}

	void clear() {data.clear();}
	bool parseLine(const char *buffer, Args* args) { return true; }
	std::string str() {
		std::string s;
		for (auto i : data)
			s += fmt::format("{}{}={}", (s.length() > 0) ? ", " : "", i.first, i.second);
		return s;
	}
};

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "DBBench::"
class DBBench : public ExperimentTask<DBBenchStats> {
	public:    //------------------------------------------------------------------
	DBBench(const char* name_, Args* args_) : ExperimentTask(name_, args_) {
		DEBUG_MSG("constructor of task {}", name);
		debug_out = args->debug_output_db_bench;
		must_not_finish = false;
	}

	void createDB() {
		auto cmd = commandCreateDB();
		spdlog::info("Creating Database. Command:\n{}", cmd);
		auto ret = std::system(cmd.c_str());
		if (ret != 0)
			throw Exception("database creation error");
	}

	protected: //------------------------------------------------------------------
	std::string commandCreateDB() {
		const char *template_cmd =
		"db_bench                                         \\\n"
		"	--db={}                                       \\\n"
		"	--options_file={}                             \\\n"
		"	--num={}                                      \\\n"
		"	--benchmarks=fillrandom                       \\\n"
		"	--perf_level=3                                \\\n"
		"	--use_direct_io_for_flush_and_compaction=true \\\n"
		"	--use_direct_reads=true                       \\\n"
		"	--cache_size={}                               \\\n"
		"	--key_size=48                                 \\\n"
		"	--value_size=43                               ";
		std::string ret = fmt::format(template_cmd,
			args->db_path,
			args->db_config_file,
			args->db_num_keys,
			args->db_cache_size);

		return ret;
	}
	virtual std::string getCmd() override {
		uint32_t duration = args->hours * 60 * 60; /*hours to seconds*/
		double   sine_b   = 0.000073 * (24.0/static_cast<double>(args->hours)); /*adjust the sine cycle*/

		const char *template_cmd =
		"db_bench                                         \\\n"
		"	--db=\"{}\"                                   \\\n"
		"	--use_existing_db=true                        \\\n"
		"	--options_file=\"{}\"                         \\\n"
		"	--num={}                                      \\\n"
		"	--key_size=48                                 \\\n"
		"	--perf_level=2                                \\\n"
		"	--stats_interval_seconds={}                   \\\n"
		"	--stats_per_interval=1                        \\\n"
		"	--benchmarks=mixgraph                         \\\n"
		"	--use_direct_io_for_flush_and_compaction=true \\\n"
		"	--use_direct_reads=true                       \\\n"
		"	--cache_size={}                               \\\n"
		"	--key_dist_a=0.002312                         \\\n"
		"	--key_dist_b=0.3467                           \\\n"
		"	--keyrange_dist_a=14.18                       \\\n"
		"	--keyrange_dist_b=-2.917                      \\\n"
		"	--keyrange_dist_c=0.0164                      \\\n"
		"	--keyrange_dist_d=-0.08082                    \\\n"
		"	--keyrange_num=30                             \\\n"
		"	--value_k=0.2615                              \\\n"
		"	--value_sigma=25.45                           \\\n"
		"	--iter_k=2.517                                \\\n"
		"	--iter_sigma=14.236                           \\\n"
		"	--mix_get_ratio=0.83                          \\\n"
		"	--mix_put_ratio=0.14                          \\\n"
		"	--mix_seek_ratio=0.03                         \\\n"
		"	--sine_mix_rate_interval_milliseconds=5000    \\\n"
		"	--duration={}                                 \\\n"
		"	--sine_a=1000                                 \\\n"
		"	--sine_b={}                                   \\\n"
		"	--sine_d=4500            2>&1                 ";
		std::string ret = fmt::format(template_cmd,
			args->db_path,
			args->db_config_file,
			args->db_num_keys,
			args->stats_interval,
			args->db_cache_size,
			duration,
			sine_b);

		spdlog::info("Executing db_bench. Command:\n{}", ret);
		return ret;
	}

	virtual StatsParseLine parseThisLine(const char* buffer) override {
		if (std::regex_search(buffer, std::regex("thread [0-9]+:.*"))
		||  std::regex_search(buffer, std::regex("Interval writes:.*"))
		||  std::regex_search(buffer, std::regex("Interval WAL:.*"))) {
			return USE_LINE;
		}

		if (std::regex_search(buffer, std::regex("Interval stall:.*")))
			return USE_LINE_END;

		return DONT_USE;
	}
};

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "SystemStats::"
class SystemStats {
	public:
	std::map<std::string, std::string> data;
	bool debug_out = false;

	SystemStats(bool debug_out=false) {
		getStatsLoad(debug_out);
	}
	SystemStats(const SystemStats& src) {*this = src;}
	SystemStats& operator= (const SystemStats& src) {
		data = src.data;
		debug_out = src.debug_out;
		return *this;
	}

	std::string str() {
		std::string s;
		for (auto i : data)
			s += fmt::format("{}{}={}", (s.length() > 0) ? ", " : "", i.first, i.second);
		return s;
	}

	private: //---------------------------------------------------------------------
	void getStatsLoad(bool debug_out) {
		auto flags = std::regex_constants::match_any;
		std::string ret;
		std::string aux;
		std::smatch sm;
		if (Subprocess("uptime", "uptime").getAll(ret) > 0) {
			std::regex_search(ret, sm, std::regex("load average:\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+)"), flags);
			if( sm.size() >= 4 ){
				data["load1"]  = str_replace(aux, sm.str(1), ',', '.');
				data["load5"]  = str_replace(aux, sm.str(2), ',', '.');
				data["load15"] = str_replace(aux, sm.str(3), ',', '.');
				DEBUG_OUT(debug_out, "line parsed    : {}", ret);
			} else {
				DEBUG_OUT(debug_out, "line not parsed: {}", ret);
			}
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "IOStats::"
class IOStats {
	public:
	std::map<std::string, std::string> data;

	IOStats() {}
	IOStats(const char* buffer, Args* args) {
		//Device            r/s     w/s     rMB/s     wMB/s   rrqm/s   wrqm/s  %rrqm  %wrqm r_await w_await aqu-sz rareq-sz wareq-sz  svctm  %util
		//nvme0n1          0,00    0,00      0,00      0,00     0,00     0,00   0,00   0,00    0,00    0,00   0,00     0,00     0,00   0,00   0,00
		std::vector<std::string> columns;
		std::vector<std::string> values;

		auto columns_s = split_columns(columns, buffer, "Device");
		auto values_s = split_columns(values, buffer, args->io_device.c_str());

		if (columns_s == 0 || columns_s != values_s)
			throw Exception("invalid iostats");

		std::string aux;
		for (int i=0; i < columns_s; i++) {
			data[columns[i]] = str_replace(aux, values[i], ',', '.');
		}
	}
	IOStats(const IOStats& src) {*this = src;}
	IOStats& operator= (const IOStats& src) {
		data = src.data;
		return *this;
	}
	std::string str() {
		std::string s;
		for (auto i : data)
			s += fmt::format("{}{}={}", (s.length() > 0) ? ", " : "", i.first, i.second);
		return s;
	}
};

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "IOStatsThread::"
class IOStatsThread : public ExperimentTask<IOStats> {
	bool begin_use_stats = false;

	public:    //------------------------------------------------------------------
	IOStatsThread(const char* name_, Args* args_) : ExperimentTask(name_, args_) {
		DEBUG_MSG("constructor of task {}", name);
		debug_out = args->debug_output_iostat;
		must_not_finish = true;

		devCheck(args_->io_device);

		launchThread();
	}

	protected: //------------------------------------------------------------------
	void devCheck(std::string& device) {
		if (device.length() == 0)
			throw Exception("io_device not specified");

		std::string filename("/dev/"); filename += device;
		struct stat s;

		if (stat(filename.c_str(), &s) != 0)
			throw Exception(fmt::format("failed to read device {}", filename));
	}

	virtual std::string getCmd() override {
		return fmt::format("iostat -xm {} {}", args->stats_interval, args->io_device);
	}

	virtual StatsParseLine parseThisLine(const char* buffer) override {
		std::regex header_line(fmt::format("(Device)\\s+", args->io_device));
		std::regex device_line(fmt::format("({})\\s+", args->io_device));

		if (begin_use_stats) {
			if (std::regex_search(buffer, header_line))
				return USE_LINE;

			if (std::regex_search(buffer, device_line))
				return USE_LINE_END;
		} else {
			if (std::regex_search(buffer, device_line))
				begin_use_stats = true;
		}

		return DONT_USE;
	}
};

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "Program::"
class Program {
	std::unique_ptr<Args> args;

	public: //---------------------------------------------------------------------
	Program(int argc, char** argv) {
		args.reset(new Args(argc, argv));
	}

	void main() {
		DEBUG_MSG("initialized");
		DBBench db_bench("dbbench", args.get());
		IOStatsThread iostat("iostat", args.get());

		if (args->db_create)
			db_bench.createDB();

		db_bench.launchThread();

		int force_finish = 0;

		DBBenchStats stats_db_bench;
		IOStats stats_io;
		while (!db_bench.finished()) {

			iostat.consumeStats(stats_io);

			if (db_bench.consumeStats(stats_db_bench)) {
				spdlog::info("system   stats: {}", SystemStats(args->debug_output).str());
				spdlog::info("I/O      stats: {}", stats_io.str());
				spdlog::info("db_bench stats: {}", stats_db_bench.str());
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(500));

			if (++force_finish > 30) {
				DEBUG_MSG("force finish");
				break;
			}
		}

		iostat.stop();
		db_bench.stop();
		DEBUG_MSG("finished");
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

void signal_handler(int signal) {
	auto group = getpgrp();
	spdlog::warn("received signal {}, process group = {}", signal, group);
	std::signal(signal, SIG_DFL);
	killpg(group, signal);
}

////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
	spdlog::info("Initializing program {}", argv[0]);

	if (setpgrp() < 0) {
		spdlog::error("failed to create process group");
		return 1;
	}
	std::signal(SIGTERM, signal_handler);
	std::signal(SIGSEGV, signal_handler);
	std::signal(SIGINT, signal_handler);
	std::signal(SIGILL, signal_handler);
	std::signal(SIGABRT, signal_handler);
	std::signal(SIGFPE, signal_handler);

	try {

		Program(argc, argv).main();

	} catch (const std::exception& e) {
		spdlog::error(e.what());
		std::raise(SIGTERM);
		return 1;
	}

	spdlog::info("exit OK!");
	return 0;
}
