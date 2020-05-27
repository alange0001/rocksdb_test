
#include <string>
#include <vector>
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
#include "experiment_task.h"

using std::string;
using std::vector;
using std::chrono::milliseconds;
using std::runtime_error;
using std::regex;
using std::regex_search;
using std::unique_ptr;
using std::runtime_error;
using fmt::format;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "DBBench::"

class DBBench : public ExperimentTask {
	Args* args;
	uint number;

	public:    //------------------------------------------------------------------
	DBBench(Clock* clock_, Args* args_, uint number_) : ExperimentTask(format("db_bench[{}]", number_), clock_), args(args_), number(number_) {
		DEBUG_MSG("constructor");

		if (args->db_create)
			createDB();

		string cmd(get_cmd_run());
		spdlog::info("Executing {}. Command:\n{}", name, cmd);
		process.reset(new ProcessController(
			name.c_str(),
			cmd.c_str(),
			[this](const char* v){this->stdoutHandler(v);},
			[this](const char* v){this->default_stderr_handler(v);},
			false
			));
	}
	~DBBench() {}

	private: //------------------------------------------------------------------
	void createDB() {
		string stats =
			format("    --statistics=0                                \\\n") +
			format("    --stats_per_interval=1                        \\\n") +
			format("    --stats_interval_seconds=60                   \\\n") +
			format("    --histogram=1                                 \\\n");

		string cmd =
			format("db_bench --benchmarks=fillrandom                  \\\n") +
			format("    --use_existing_db=0                           \\\n") +
			format("    --disable_auto_compactions=1                  \\\n") +
			format("    --sync=0                                      \\\n") +
			get_params_bulkload() +
			format("    --threads=1                                   \\\n") +
			format("    --memtablerep=vector                          \\\n") +
			format("    --allow_concurrent_memtable_write=false       \\\n") +
			format("    --disable_wal=1                               \\\n") +
			format("    --seed=$( date +%s )                          \\\n") +
			stats +
			format("    2>&1 ");
		spdlog::info("Bulkload {}. Command:\n{}", name, cmd);
		auto ret = std::system(cmd.c_str());
		if (ret != 0)
			throw runtime_error("database bulkload error");

		cmd =
			format("db_bench --benchmarks=compact                     \\\n") +
			format("    --use_existing_db=1                           \\\n") +
			format("    --disable_auto_compactions=1                  \\\n") +
			format("    --sync=0                                      \\\n") +
			get_params_w() +
			format("    --threads=1                                   \\\n") +
			stats +
			format("    2>&1 ");
		spdlog::info("Compact {}. Command:\n{}", name, cmd);
		ret = std::system(cmd.c_str());
		if (ret != 0)
			throw runtime_error("database compact error");
	}

	string get_const_params() {
		string config;
		if (args->db_config_file[number].length() > 0)
			config = fmt::format("	--options_file=\"{}\" \\\n", args->db_config_file[number]);
		string ret =
			format("    --db=\"{}\"                                   \\\n", args->db_path[number]) +
			format("    --wal_dir=\"{}\"                              \\\n", args->db_path[number]) +
			config +
			format("    --num={}                                      \\\n", args->db_num_keys[number]) +
			format("    --num_levels=6                                \\\n") +
			format("    --key_size={}                                 \\\n", 20 /* mixgraph: 48 */) +
			format("    --value_size={}                               \\\n", 400 /* mixgraph 43 */) +
			format("    --block_size={}                               \\\n", 8 * 1024) +
			format("    --cache_size={}                               \\\n", args->db_cache_size[number]) +
			format("    --cache_numshardbits=6                        \\\n") +
			format("    --compression_max_dict_bytes={}               \\\n", 0) +
			format("    --compression_ratio=0.5                       \\\n") +
			format("    --compression_type=\"{}\"                     \\\n", "zstd") +
			format("    --level_compaction_dynamic_level_bytes=true   \\\n") +
			format("    --bytes_per_sync={}                           \\\n", 8 * 1024 * 1024) +
			format("    --cache_index_and_filter_blocks=0             \\\n") +
			format("    --pin_l0_filter_and_index_blocks_in_cache=1   \\\n") +
			format("    --benchmark_write_rate_limit={}               \\\n", 0) +
			format("                                                  \\\n") +
			format("    --hard_rate_limit=3                           \\\n") +
			format("    --rate_limit_delay_max_milliseconds=1000000   \\\n") +
			format("    --write_buffer_size={}                        \\\n", 128 * 1024 * 1024) +
			format("    --target_file_size_base={}                    \\\n", 128 * 1024 * 1024) +
			format("    --max_bytes_for_level_base={}                 \\\n", 1 * 1024 * 1024 * 1024) +
			format("                                                  \\\n") +
			format("    --verify_checksum=1                           \\\n") +
			format("    --delete_obsolete_files_period_micros={}      \\\n", 60 * 1024 * 1024) +
			format("    --max_bytes_for_level_multiplier=8            \\\n") +
			format("                                                  \\\n") +
		//	format("    --statistics=0                                \\\n") +
		//	format("    --stats_per_interval=1                        \\\n") +
		//	format("    --stats_interval_seconds=60                   \\\n") +
		//	format("    --histogram=1                                 \\\n") +
		//	format("                                                  \\\n") +
			format("    --memtablerep=skip_list                       \\\n") +
			format("    --bloom_bits=10                               \\\n") +
			format("    --open_files=-1                               \\\n");
		return ret;
	}
	string get_params_bulkload() {
		string ret =
			get_const_params() +
			format("    --max_background_compactions=16               \\\n") +
			format("    --max_write_buffer_number=8                   \\\n") +
			format("    --allow_concurrent_memtable_write=false       \\\n") +
			format("    --max_background_flushes=7                    \\\n") +
			format("    --level0_file_num_compaction_trigger={}       \\\n", 10 * 1024 * 1024) +
			format("    --level0_slowdown_writes_trigger={}           \\\n", 10 * 1024 * 1024) +
			format("    --level0_stop_writes_trigger={}               \\\n", 10 * 1024 * 1024);
		return ret;
	}
	string get_params_w() {
		string ret =
			get_const_params() +
			format("    --level0_file_num_compaction_trigger=4        \\\n") +  //l0_config
			format("    --level0_stop_writes_trigger=20               \\\n") +  //l0_config
			format("    --max_background_compactions=16               \\\n") +
			format("    --max_write_buffer_number=8                   \\\n") +
			format("    --max_background_flushes=7                    \\\n");
		return ret;
	}

	string get_cmd_run() {
#		define returnCommand(name) \
			if (args->db_benchmark[number] == #name) \
				return get_cmd_##name();

		returnCommand(readwhilewriting)
		returnCommand(mixgraph)
#		undef returnCommand

		throw runtime_error(format("invalid benchmark name: \"{}\"", args->db_benchmark[number]));
	}
	string get_cmd_readwhilewriting() {
		uint32_t duration_s = args->duration * 60; /*minutes to seconds*/

		string ret =
			format("db_bench --benchmarks=readwhilewriting            \\\n") +
			format("    --duration={}                                 \\\n", duration_s) +
			get_params_w() +
			format("    --use_existing_db=true                        \\\n") +
			format("    --threads={}                                  \\\n", args->db_threads[number]) +
			format("                                                  \\\n") +
			format("    --perf_level=2                                \\\n") +
			format("    --stats_interval_seconds={}                   \\\n", args->stats_interval) +
			format("    --stats_per_interval=1                        \\\n") +
			format("                                                  \\\n") +
			format("    --sync={}                                     \\\n", 1 /*syncval*/) +
			format("    --merge_operator=\"put\"                      \\\n") +
			format("    --seed=$( date +%s )                          \\\n") +
			format("    {}  2>&1 ", args->db_bench_params[number]);
		return ret;
	}
	string get_cmd_mixgraph() {
		uint32_t duration_s = args->duration * 60; /*minutes to seconds*/
		double   sine_b   = 0.000073 * 24.0 * 60.0 * ((double)args->db_sine_cycles[number] / (double)args->duration); /*adjust the sine cycle*/
		double   sine_c   = sine_b * (double)args->db_sine_shift[number] * 60.0;

		string ret =
			format("db_bench --benchmarks=mixgraph                    \\\n") +
			format("    --duration={}                                 \\\n", duration_s) +
			get_params_w() +
			format("    --use_existing_db=true                        \\\n") +
			format("    --threads={}                                  \\\n", args->db_threads[number]) +
			format("                                                  \\\n") +
			format("    --perf_level=2                                \\\n") +
			format("    --stats_interval_seconds={}                   \\\n", args->stats_interval) +
			format("    --stats_per_interval=1                        \\\n") +
			format("                                                  \\\n") +
			format("    --key_dist_a=0.002312                         \\\n") +
			format("    --key_dist_b=0.3467                           \\\n") +
			format("    --keyrange_dist_a=14.18                       \\\n") +
			format("    --keyrange_dist_b=-2.917                      \\\n") +
			format("    --keyrange_dist_c=0.0164                      \\\n") +
			format("    --keyrange_dist_d=-0.08082                    \\\n") +
			format("    --keyrange_num=30                             \\\n") +
			format("    --value_k=0.2615                              \\\n") +
			format("    --value_sigma=25.45                           \\\n") +
			format("    --iter_k=2.517                                \\\n") +
			format("    --iter_sigma=14.236                           \\\n") +
			format("    --mix_get_ratio=0.83                          \\\n") +
			format("    --mix_put_ratio=0.14                          \\\n") +
			format("    --mix_seek_ratio=0.03                         \\\n") +
			format("    --sine_mix_rate_interval_milliseconds=5000    \\\n") +
			format("    --sine_b={}                                   \\\n", sine_b) +
			format("    --sine_c={}                                   \\\n", sine_c) +
			format("    {} {}  2>&1 ", args->db_mixgraph_params, args->db_bench_params[number]);
		return ret;
	}

	uint64_t ops = 0;
	double ops_per_s = 0;
	void stdoutHandler(const char* buffer) {
		auto flags = std::regex_constants::match_any;
		std::cmatch cm;

		spdlog::info("Task {}, stdout: {}", name, str_replace(buffer, '\n', ' '));

		regex_search(buffer, cm, regex("thread ([0-9]+): \\(([0-9.]+),([0-9.]+)\\) ops and \\(([0-9.]+),([0-9.]+)\\) ops/second in \\(([0-9.]+),([0-9.]+)\\) seconds.*"), flags);
		if( cm.size() >= 8 ){
			ops += parseUint64(cm.str(2), true, 0, "invalid ops");
			ops_per_s += parseDouble(cm.str(4), true, 0, "invalid ops_per_s");
			data["ops"] = format("{}", ops);
			data["ops_per_s"] = format("{:.1f}", ops_per_s);
			data[format("ops[{}]", cm.str(1))] = cm.str(2);
			data[format("ops_per_s[{}]", cm.str(1))] = cm.str(4);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);
		}
		regex_search(buffer, cm, regex("Interval writes: ([0-9.]+[KMGT]*) writes, ([0-9.]+[KMGT]*) keys, ([0-9.]+[KMGT]*) commit groups, ([0-9.]+[KMGT]*) writes per commit group, ingest: ([0-9.]+) [KMGT]*B, ([0-9.]+) [KMGT]*B/s.*"), flags);
		if( cm.size() >= 7 ){
			data["writes"] = cm.str(1);
			data["written_keys"] = cm.str(2);
			data["written_commit_groups"] = cm.str(3);
			data["ingest_MBps"] = cm.str(5);
			data["ingest_MBps"] = cm.str(6);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);
		}
		regex_search(buffer, cm, regex("Interval WAL: ([0-9.]+[KMGT]*) writes, ([0-9.]+[KMGT]*) syncs, ([0-9.]+[KMGT]*) writes per sync, written: ([0-9.]+) [KMGT]*B, ([0-9.]+) [KMGT]*B/s.*"), flags);
		if( cm.size() >= 5 ){
			data["WAL_writes"] = cm.str(1);
			data["WAL_syncs"] = cm.str(2);
			data["WAL_written_MB"] = cm.str(4);
			data["WAL_written_MBps"] = cm.str(5);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);
		}
		regex_search(buffer, cm, regex("Interval stall: ([0-9:.]+) H:M:S, ([0-9.]+) percent.*"), flags);
		if( cm.size() >= 3 ){
			data["stall"] = cm.str(1);
			data["stall_percent"] = cm.str(2);
			//DEBUG_OUT(args->debug_output_db_bench, "line parsed    : {}", buffer);

			print();
			data.clear();
			ops = 0;
			ops_per_s = 0;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "AccessTime3::"

class AccessTime3 : public ExperimentTask {
	Args* args;
	uint number;

	public:    //------------------------------------------------------------------
	AccessTime3(Clock* clock_, Args* args_, uint number_) : ExperimentTask(format("access_time3[{}]", number_), clock_), args(args_), number(number_) {
		DEBUG_MSG("constructor");

		string cmd( getCmd() );
		process.reset(new ProcessController(
			name.c_str(),
			cmd.c_str(),
			[this](const char* v){this->stdoutHandler(v);},
			[this](const char* v){this->default_stderr_handler(v);},
			false
			));

	}
	~AccessTime3() {}

	string getCmd() {
		const char *template_cmd =
		"access_time3                                     \\\n"
		"   --stats_interval={}                           \\\n"
		"   --log_time_prefix=false                       \\\n"
		"	--filename=\"{}\"                             \\\n"
		"	--create_file=false                           \\\n"
		"	--block_size={}                               \\\n"
		"	--command_script=\"{}\"                       \\\n"
		"	{} 2>&1";
		string ret( format(template_cmd,
				args->stats_interval,
				args->at_file[number],
				args->at_block_size[number],
				args->at_script[number],
				args->at_params[number]) );

		spdlog::info("Executing {}. Command:\n{}", name, ret);
		return ret;
	}

	void stdoutHandler(const char* buffer) {
		std::cmatch cm;

		spdlog::info("Task {}, stdout: {}", name, str_replace(buffer, '\n', ' '));

		regex_search(buffer, cm, regex("STATS: \\{[^,]+, ([^\\}]+)\\}"));
		if (cm.size() > 1) {
			spdlog::info("Task {}, STATS: {} \"time\":\"{}\", {} {}", name, "{", clock->s(), cm.str(1), "}");
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "SystemStats::"

class SystemStats : public ExperimentTask {
	Args* args;
	bool debug_out = false;

	public: //----------------------------------------------------------------------
	SystemStats(Clock* clock_, Args* args_) : ExperimentTask("systemstats", clock_), args(args_) {
		DEBUG_MSG("constructor");
		string cmd(format("while true; do sleep {} && uptime; done", args->stats_interval));
		process.reset(new ProcessController(
			name.c_str(),
			cmd.c_str(),
			[this](const char* v){this->stdoutHandler(v);},
			[this](const char* v){this->default_stderr_handler(v);},
			args->debug_output
			));
	}
	~SystemStats() {}

	private: //---------------------------------------------------------------------
	void stdoutHandler(const char* buffer) {
		string aux;
		std::cmatch cm;
		regex_search(buffer, cm, regex("load average:\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+)"));
		if( cm.size() >= 4 ){
			data.clear();
			data["load1"]  = str_replace(aux, cm.str(1), ',', '.');
			data["load5"]  = str_replace(aux, cm.str(2), ',', '.');
			data["load15"] = str_replace(aux, cm.str(3), ',', '.');

			print();
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "IOStat::"

class IOStat : public ExperimentTask {
	Args* args;
	string io_device;

	bool first = true;
	vector<string> columns;

	public: //----------------------------------------------------------------------
	IOStat(Clock* clock_, Args* args_) : ExperimentTask("iostat", clock_), args(args_) {
		DEBUG_MSG("constructor");
		io_device = args->io_device;
		devCheck();
		string cmd(format("iostat -xm {} {}", args->stats_interval, io_device));
		process.reset(new ProcessController(
			name.c_str(),
			cmd.c_str(),
			[this](const char* v){this->stdoutHandler(v);},
			[this](const char* v){this->default_stderr_handler(v);},
			args->debug_output_iostat
			));
	}
	~IOStat() {}

	private: //---------------------------------------------------------------------
	void stdoutHandler(const char* buffer) {
		//Device            r/s     w/s     rMB/s     wMB/s   rrqm/s   wrqm/s  %rrqm  %wrqm r_await w_await aqu-sz rareq-sz wareq-sz  svctm  %util
		//nvme0n1          0,00    0,00      0,00      0,00     0,00     0,00   0,00   0,00    0,00    0,00   0,00     0,00     0,00   0,00   0,00

		if (regex_search(buffer, regex(format("^({})\\s+", io_device)))) {
			if (first) {
				first = false;
			} else {
				vector<string> values;
				auto columns_s = columns.size();
				auto values_s = split_columns(values, buffer, io_device.c_str());

				if (values_s == 0 || values_s != columns_s) {
					throw runtime_error(format("invalid iostat data: {}", buffer));
				}

				data.clear();
				string aux;
				for (int i=0; i < columns_s; i++) {
					data[columns[i]] = str_replace(aux, values[i], ',', '.');
				}

				print();
			}

		} else if (first && regex_search(buffer, regex("^(Device)\\s+"))) {
			if (columns.size() > 0)
				throw runtime_error("iostat header read twice");
			if (split_columns(columns, buffer, "Device") == 0)
				throw runtime_error(format("invalid iostat header: {}", buffer));
		}
	}

	void devCheck() {
		if (args->io_device.length() == 0)
			throw runtime_error("io_device not specified");

		string filename("/dev/"); filename += args->io_device;
		struct stat s;

		if (stat(filename.c_str(), &s) != 0)
			throw runtime_error(format("failed to read device {}", filename));
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Program::"

class Program {
	static Program*   this_;
	unique_ptr<Args>  args;
	unique_ptr<Clock> clock;

	unique_ptr<unique_ptr<DBBench>[]>     dbbench_list;
	unique_ptr<unique_ptr<AccessTime3>[]> at_list;
	unique_ptr<IOStat>      iostat;
	unique_ptr<SystemStats> sysstat;

	public: //---------------------------------------------------------------------
	Program() {
		DEBUG_MSG("constructor");
		Program::this_ = this;
		if (setpgrp() < 0) {
			spdlog::error("failed to create process group");
			exit(EXIT_FAILURE);
		}
		std::signal(SIGTERM, Program::signalWrapper);
		std::signal(SIGSEGV, Program::signalWrapper);
		std::signal(SIGINT,  Program::signalWrapper);
		std::signal(SIGILL,  Program::signalWrapper);
		std::signal(SIGABRT, Program::signalWrapper);
		std::signal(SIGFPE,  Program::signalWrapper);
	}
	~Program() {
		DEBUG_MSG("destructor");
		std::signal(SIGTERM, SIG_DFL);
		std::signal(SIGSEGV, SIG_DFL);
		std::signal(SIGINT,  SIG_DFL);
		std::signal(SIGILL,  SIG_DFL);
		std::signal(SIGABRT, SIG_DFL);
		std::signal(SIGFPE,  SIG_DFL);
		Program::this_ = nullptr;
	}

	int main(int argc, char** argv) noexcept {
		DEBUG_MSG("initialized");
		spdlog::info("rocksdb_test version: 1.3");
		try {
			args.reset(new Args(argc, argv));
			clock.reset(new Clock());

			auto num_dbs = args->num_dbs;
			auto num_at  = args->num_at;

			dbbench_list.reset(new unique_ptr<DBBench>[num_dbs]);
			for (uint32_t i=0; i<num_dbs; i++) {
				dbbench_list[i].reset(new DBBench(clock.get(), args.get(), i));
			}

			at_list.reset(new unique_ptr<AccessTime3>[num_at]);
			for (uint32_t i=0; i<num_at; i++) {
				at_list[i].reset(new AccessTime3(clock.get(), args.get(), i));
			}

			clock->reset();
			iostat.reset(new IOStat(clock.get(), args.get()));
			sysstat.reset(new SystemStats(clock.get(), args.get()));

			bool stop = false;
			while ( !stop &&
			        (iostat.get()  != nullptr && iostat->isActive())  &&
			        (sysstat.get() != nullptr && sysstat->isActive()) )
			{
				for (uint32_t i=0; i<num_dbs; i++) {
					if (dbbench_list.get() == nullptr || dbbench_list[i].get() == nullptr || !dbbench_list[i]->isActive()) {
						stop = true;
						break;
					}
				}
				if (stop) break;
				for (uint32_t i=0; i<num_at; i++) {
					if (at_list.get() == nullptr || at_list[i].get() == nullptr || !at_list[i]->isActive()) {
						stop = true;
						break;
					}
				}
				if (stop) break;
				std::this_thread::sleep_for(milliseconds(500));
			}

			resetAll();

		} catch (const std::exception& e) {
			spdlog::critical(e.what());
			resetAll();
			spdlog::info("exit(1)");
			return 1;
		}
		spdlog::info("exit(0)");
		return 0;
	}

	private: //--------------------------------------------------------------------
	void resetAll() noexcept {
		DEBUG_MSG("destroy tasks");
		dbbench_list.reset(nullptr);
		at_list.reset(nullptr);
		iostat.reset(nullptr);
		sysstat.reset(nullptr);
		std::this_thread::sleep_for(milliseconds(300));
	}

	static void signalWrapper(int signal) noexcept {
		if (Program::this_)
			Program::this_->signalHandler(signal);
	}
	void signalHandler(int signal) noexcept {
		auto group = getpgrp();
		spdlog::warn("received signal {}, process group = {}", signal, group);

		resetAll();

		fflush(stdout);
		std::signal(signal, SIG_DFL);
		killpg(group, signal);
	}
};
Program* Program::this_;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

int main(int argc, char** argv) {
	Program p;
	return p.main(argc, argv);
}
