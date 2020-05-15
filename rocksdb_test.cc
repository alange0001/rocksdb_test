
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
#define __CLASS__ "DBBench::"

class DBBench : public ExperimentTask {
	Args* args;

	public:    //------------------------------------------------------------------
	DBBench(Clock* clock_, Args* args_) : ExperimentTask("dbbench", clock_), args(args_) {
		DEBUG_MSG("constructor");

		if (args->db_create)
			createDB();

		std::string cmd(getCmd());
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
			throw std::runtime_error("database creation error");
	}
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
	std::string getCmd() {
		uint32_t duration_s = args->duration * 60; /*minutes to seconds*/
		double   sine_b   = 0.000073 * ((24.0 * 60.0) / ((double)args->duration / (double)args->cycles)); /*adjust the sine cycle*/

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
		"	{}  2>&1";
		std::string ret = fmt::format(template_cmd,
			args->db_path,
			args->db_config_file,
			args->db_num_keys,
			args->stats_interval,
			args->db_cache_size,
			duration_s,
			sine_b,
			args->db_bench_params);

		spdlog::info("Executing db_bench. Command:\n{}", ret);
		return ret;
	}

	void stdoutHandler(const char* buffer) {
		auto flags = std::regex_constants::match_any;
		std::cmatch cm;

		spdlog::info("Task {}, stdout: {}", name, str_replace(buffer, '\n', ' '));

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

			print();
			data.clear();
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
		std::string cmd(fmt::format("while true; do sleep {} && uptime; done", args->stats_interval));
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
		std::string aux;
		std::cmatch cm;
		std::regex_search(buffer, cm, std::regex("load average:\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+)"));
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

	bool first = true;
	std::vector<std::string> columns;

	public: //----------------------------------------------------------------------
	IOStat(Clock* clock_, Args* args_) : ExperimentTask("iostat", clock_), args(args_) {
		DEBUG_MSG("constructor");
		devCheck();
		std::string cmd(fmt::format("iostat -xm {} {}", args->stats_interval, args->io_device));
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

		if (std::regex_search(buffer, std::regex(fmt::format("^({})\\s+", args->io_device)))) {
			if (first) {
				first = false;
			} else {
				std::vector<std::string> values;
				auto columns_s = columns.size();
				auto values_s = split_columns(values, buffer, args->io_device.c_str());

				if (values_s == 0 || values_s != columns_s) {
					throw std::runtime_error(fmt::format("invalid iostat data: {}", buffer));
				}

				data.clear();
				std::string aux;
				for (int i=0; i < columns_s; i++) {
					data[columns[i]] = str_replace(aux, values[i], ',', '.');
				}

				print();
			}

		} else if (first && std::regex_search(buffer, std::regex("^(Device)\\s+"))) {
			if (columns.size() > 0)
				throw std::runtime_error("iostat header read twice");
			if (split_columns(columns, buffer, "Device") == 0)
				throw std::runtime_error(fmt::format("invalid iostat header: {}", buffer));
		}
	}

	void devCheck() {
		if (args->io_device.length() == 0)
			throw std::runtime_error("io_device not specified");

		std::string filename("/dev/"); filename += args->io_device;
		struct stat s;

		if (stat(filename.c_str(), &s) != 0)
			throw std::runtime_error(fmt::format("failed to read device {}", filename));
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Program::"

class Program {
	static Program* this_;
	Args args;
	Clock clock;

	std::unique_ptr<DBBench>     dbbench;
	std::unique_ptr<IOStat>      iostat;
	std::unique_ptr<SystemStats> sysstat;

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
		try {
			args.parseArgs(argc, argv);

			dbbench.reset(new DBBench(&clock, &args));
			clock.reset();
			iostat.reset(new IOStat(&clock, &args));
			sysstat.reset(new SystemStats(&clock, &args));

			while (
				(dbbench.get() != nullptr && dbbench->isActive()) &&
				(iostat.get()  != nullptr && iostat->isActive())  &&
				(sysstat.get() != nullptr && sysstat->isActive()) )
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
		dbbench.reset(nullptr);
		iostat.reset(nullptr);
		sysstat.reset(nullptr);
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
