
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <regex>

#include <chrono>
#include <atomic>
#include <thread>

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

std::atomic_flag stats_queue_lock = ATOMIC_FLAG_INIT;

class DBBench {
	Args* args;
	std::queue<DBBenchStats> stats_queue;

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

public:
	bool finished = false;
	DBBench(Args* args_) : args(args_) {}

	void create() {
		auto cmd = getCommandCreate();
		auto ret = std::system(cmd.c_str());
		if (ret != 0)
			throw new std::string("database creation error");
	}

	void run() {
		const int max_buffer = 512;
		char buffer[max_buffer];
		auto cmd = getCommandRun();

		DBBenchStats stats;
		FILE* f = popen(cmd.c_str(), "r");
		if (f == nullptr)
			throw new std::string("error executing db_bench");

		while (fgets(buffer, max_buffer, f)){
			buffer[max_buffer -1] = '\0';
			if (stats.parseLine(buffer)) { // if last line was parsed
				while (stats_queue_lock.test_and_set(std::memory_order_acquire));
				stats_queue.push(stats);
				stats_queue_lock.clear();
				stats.clear();
			}
		}

		auto exit_code = pclose(f);
		finished = true;
		if (exit_code != 0)
			throw new std::string(fmt::format("db_bench exit error {}: {}", exit_code, buffer));
	}

	bool getStats(std::string& ret) {
		while (stats_queue_lock.test_and_set(std::memory_order_acquire));
		if (stats_queue.size() > 0) {
			DBBenchStats s = stats_queue.front();
			stats_queue.pop();
			stats_queue_lock.clear();
			ret = s.str();
			return true;
		}
		stats_queue_lock.clear();
		return false;
	}

};

////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
	spdlog::info("Initializing program {}", argv[0]);

	try {
		Args args(argc, argv);
		DBBench db_bench(&args); auto db_bench_ptr = &db_bench;
		if (args.db_create)
			db_bench.create();

		std::thread thread_db_bench ( [db_bench_ptr] {db_bench_ptr->run();} );
		std::string stats_db_bench;
		while (!db_bench.finished) {
			if (db_bench.getStats(stats_db_bench)) {
				spdlog::info("db_bench stats: {}", stats_db_bench);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		thread_db_bench.join();
	}
	catch (std::string* str_error) {
		spdlog::error(*str_error);
		return 1;
	}

	spdlog::info("exit 0");
	return 0;
}
