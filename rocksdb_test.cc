
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <regex>

#include <chrono>
#include <atomic>
#include <thread>

#include <stdexcept>

#include <csignal>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "args.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////////

struct DBBenchStats {
	std::map<std::string, std::string> stats;

	DBBenchStats() {}
	DBBenchStats(const DBBenchStats& src) {stats = src.stats;}
	DBBenchStats& operator= (const DBBenchStats& src) {stats = src.stats; return *this;}
	void clear() {stats.clear();}
	bool parseLine(const char *buffer) {
		std::cmatch cm;

		std::regex_match(buffer, cm, std::regex("\\s*Interval writes: ([0-9.]+) writes, ([0-9.]+) keys, ([0-9.]+) commit groups, ([0-9.]+) writes per commit group, ingest: ([0-9.]+) MB, ([0-9.]+) MB/s\\s*"));
		//spdlog::debug("cm.size() = {}", cm.size());
		if( cm.size() >= 7 ){
			stats["Writes"] = cm.str(1);
			stats["Write keys"] = cm.str(2);
			stats["Write commit groups"] = cm.str(3);
			stats["Ingest MB"] = cm.str(5);
			stats["Ingest MB/s"] = cm.str(6);
			return false;
		}
		std::regex_match(buffer, cm, std::regex("\\s*Interval WAL: ([0-9.]+) writes, ([0-9.]+) syncs, ([0-9.]+) writes per sync, written: ([0-9.]+) MB, ([0-9.]+) MB/s\\s*"));
		//spdlog::debug("cm.size() = {}", cm.size());
		if( cm.size() >= 5 ){
			stats["WAL writes"] = cm.str(1);
			stats["WAL syncs"] = cm.str(2);
			stats["WAL MB written"] = cm.str(4);
			stats["WAL MB written/s"] = cm.str(5);
			return false;
		}
		std::regex_match(buffer, cm, std::regex("\\s*Interval stall: ([0-9:.]+) H:M:S, ([0-9.]+) percent\\s*"));
		//spdlog::debug("cm.size() = {}", cm.size());
		if( cm.size() >= 3 ){
			stats["Stall"] = cm.str(1);
			stats["Stall percent"] = cm.str(2);
			return true;
		}
		return false;
	}
	std::string str() {
		std::string s;
		for (auto i : stats)
			s += fmt::format("{}{}={}", (s.length() > 0)?", ":"", i.first, i.second);
		return s;
	}
};

////////////////////////////////////////////////////////////////////////////////////

std::atomic_flag DBBench_stats_lock = ATOMIC_FLAG_INIT;

class DBBench {
	bool finished_ = false;
	std::exception_ptr exception = nullptr;
	Args* args;

	DBBenchStats stats;
	uint64_t stats_produced = 0;
	uint64_t stats_consumed = 0;

	std::string getCommandCreate() {
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

		spdlog::debug("Command Run:\n{}", ret);
		return ret;
	}

	std::string getCommandRun() {
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
		"	--stats_interval_seconds=5                    \\\n"
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
			args->db_cache_size,
			duration,
			sine_b);
		spdlog::debug("Command Run:\n{}", ret);
		return ret;
	}

	public: //---------------------------------------------------------------------
	std::thread thread;

	DBBench(Args* args_) : args(args_) {}
	~DBBench() {
		if (thread.joinable())
			thread.join();
	}

	void create() {
		auto cmd = getCommandCreate();
		auto ret = std::system(cmd.c_str());
		if (ret != 0)
			throw std::runtime_error("database creation error");
	}

	void launchThread() {
		thread = std::thread( [this]{this->threadMain();} );
	}
	void threadMain() noexcept { // secondary thread to control the db_bench and handle its output
		try {
			const int buffer_size = 512;
			char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';
			auto cmd = getCommandRun();

			Popen2 subprocess(cmd.c_str());

			DBBenchStats collect_stats;
			while (subprocess.gets(buffer, buffer_size -1)){
				//spdlog::debug("buffer={}", buffer);
				if (collect_stats.parseLine(buffer)) { // if last line was parsed
					// produce stats
					while (DBBench_stats_lock.test_and_set(std::memory_order_acquire));
					stats = collect_stats;
					stats_produced++;
					DBBench_stats_lock.clear();
					collect_stats.clear();
				}
			}

		} catch (...) {
			exception = std::current_exception();
		}
		finished_ = true;
	}

	bool consumeStats(DBBenchStats& ret) { // call from the main thread
		while (DBBench_stats_lock.test_and_set(std::memory_order_acquire));
		if (stats_consumed < stats_produced) {
			ret = stats;
			stats_consumed = stats_produced;
			DBBench_stats_lock.clear();
			return true;
		}
		DBBench_stats_lock.clear();
		return false;
	}

	bool finished() { // call from the main thread
		if (exception)
			std::rethrow_exception(exception);
		return finished_;
	}

};

////////////////////////////////////////////////////////////////////////////////////

class Program {
	std::unique_ptr<Args> args;

	public: //---------------------------------------------------------------------
	Program(int argc, char** argv) {
		spdlog::debug("Program constructor");
		args.reset(new Args(argc, argv));
	}
	~Program() {
		spdlog::debug("Program destructor");
	}

	void main() {
		DBBench db_bench(args.get());

		if (args->db_create)
			db_bench.create();

		db_bench.launchThread();

		DBBenchStats stats_db_bench;
		while (!db_bench.finished()) {

			if (db_bench.consumeStats(stats_db_bench)) {
				spdlog::info("db_bench stats: {}", stats_db_bench.str());
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////

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
	}
	catch (const std::exception& e) {
		spdlog::error(e.what());
		std::raise(SIGTERM);
		return 1;
	}

	spdlog::info("exit 0");
	return 0;
}
