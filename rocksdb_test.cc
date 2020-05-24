
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

		string cmd(getCmd());
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
		auto cmd = commandCreateDB();
		spdlog::info("Creating Database. Command:\n{}", cmd);
		auto ret = std::system(cmd.c_str());
		if (ret != 0)
			throw runtime_error("database creation error");
	}
	string commandCreateDB() {
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
		string ret = format(template_cmd,
			args->db_path[number],
			args->db_config_file[number],
			args->db_num_keys[number],
			args->db_cache_size[number]);

		return ret;
	}
	string getCmd() {
		uint32_t duration_s = args->duration * 60; /*minutes to seconds*/
		double   sine_b   = 0.000073 * 24.0 * 60.0 * ((double)args->db_sine_cycles[number] / (double)args->duration); /*adjust the sine cycle*/
		double   sine_c   = sine_b * (double)args->db_sine_shift[number] * 60.0;

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
		"	--sine_b={}                                   \\\n"
		"	--sine_c={}                                   \\\n"
		"	{}  2>&1";
		string ret = format(template_cmd,
			args->db_path[number],
			args->db_config_file[number],
			args->db_num_keys[number],
			args->stats_interval,
			args->db_cache_size[number],
			duration_s,
			sine_b,
			sine_c,
			args->db_bench_params[number]);

		spdlog::info("Executing {}. Command:\n{}", name, ret);
		return ret;
	}

	void stdoutHandler(const char* buffer) {
		auto flags = std::regex_constants::match_any;
		std::cmatch cm;

		spdlog::info("Task {}, stdout: {}", name, str_replace(buffer, '\n', ' '));

		regex_search(buffer, cm, regex("thread [0-9]+: \\(([0-9.]+),([0-9.]+)\\) ops and \\(([0-9.]+),([0-9.]+)\\) ops/second in \\(([0-9.]+),([0-9.]+)\\) seconds.*"), flags);
		if( cm.size() >= 7 ){
			data["ops"] = cm.str(1);
			data["ops_total"] = cm.str(2);
			data["ops_per_s"] = cm.str(3);
			data["ops_per_s_total"] = cm.str(4);
			data["stats_interval"] = cm.str(5);
			data["stats_total"] = cm.str(5);
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
		spdlog::info("rocksdb_test version: 1.2");
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
