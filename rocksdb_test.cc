// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#include "version.h"

#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <regex>

#include <chrono>

#include <stdexcept>
#include <filesystem>

#include <csignal>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include <alutils/string.h>
#include <alutils/process.h>
#include <alutils/socket.h>

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
#define __CLASS__ ""

unique_ptr<TmpDir> tmpdir;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "DBBench::"

class DBBench : public ExperimentTask {
	Args* args;
	uint number;
	string container_name;

	public:    //------------------------------------------------------------------
	DBBench(Clock* clock_, Args* args_, uint number_) : ExperimentTask(format("db_bench[{}]", number_), clock_, args_->warm_period * 60), args(args_), number(number_) {
		DEBUG_MSG("constructor");
		container_name = format("db_bench_{}", number_);
	}

	~DBBench() {
		DEBUG_MSG("destructor");
		try {
			alutils::command_output(format("docker rm -f {}", container_name).c_str());
			process.reset(nullptr);
		} catch (const std::exception& e) {
			spdlog::warn(e.what());
		}
	}

	void checkCreate() {
		if (args->db_create)
			createDB();
	}

	void start() {
		string cmd(get_cmd_run());
		spdlog::info("Executing {}. Command:\n{}", name, cmd);
		process.reset(new alutils::ProcessController(
			name.c_str(),
			cmd.c_str(),
			[this](const char* v){this->stdoutHandler(v);},
			[this](const char* v){this->default_stderr_handler(v);}
			));
	}

	private: //------------------------------------------------------------------
	void createDB() {
		string stats =
			format("    --statistics=0                                \\\n") +
			format("    --stats_per_interval=1                        \\\n") +
			format("    --stats_interval_seconds=60                   \\\n") +
			format("    --histogram=1                                 \\\n");

		string cmd = get_docker_cmd() +
			format("  db_bench --benchmarks=fillrandom                \\\n") +
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

		cmd = get_docker_cmd() +
			format("  db_bench --benchmarks=compact                   \\\n") +
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

	string get_docker_cmd() {
		string config;
		if (args->rocksdb_config_file.length() > 0)
			config = fmt::format("  -v \"{}\":/rocksdb.options \\\n", tmpdir->getFileCopy(args->rocksdb_config_file).c_str());
		string ret =
			format("docker run --name=\"{}\" -t --rm                  \\\n", container_name) +
			format("  --ulimit nofile=1048576:1048576                 \\\n") +
			format("  --user=\"{}\"                                   \\\n", getuid()) +
			format("  -v \"{}\":/workdata                             \\\n", args->db_path[number]) +
			format("  -v {}:/tmp/host                                 \\\n", tmpdir->getContainerDir(container_name).c_str())+
			config +
			format("  {}                                              \\\n", args->docker_params) +
			format("  {}                                              \\\n", args->docker_image);
		return ret;
	}

	string get_const_params() {
		string config;
		if (args->rocksdb_config_file.length() > 0)
			config = fmt::format("    --options_file=\"/rocksdb.options\" \\\n");
		string ret =
			format("    --db=\"/workdata\"                            \\\n") +
			format("    --wal_dir=\"/workdata\"                       \\\n") +
			config +
			format("    --num={}                                      \\\n", args->db_num_keys[number]) +
			format("    --num_levels=6                                \\\n", args->db_num_levels[number]) +
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
				return get_cmd_##name()

		returnCommand(readwhilewriting);
		returnCommand(readrandomwriterandom);
		returnCommand(mixgraph);
#		undef returnCommand

		throw runtime_error(format("invalid benchmark name: \"{}\"", args->db_benchmark[number]));
	}

	string get_cmd_readwhilewriting() {
		uint32_t duration_s = args->duration * 60; /*minutes to seconds*/

		string ret = get_docker_cmd() +
			format("  db_bench --benchmarks=readwhilewriting          \\\n") +
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

	string get_cmd_readrandomwriterandom() {
		uint32_t duration_s = args->duration * 60; /*minutes to seconds*/

		string ret = get_docker_cmd() +
			format("  db_bench --benchmarks=readrandomwriterandom     \\\n") +
			format("    --duration={}                                 \\\n", duration_s) +
			get_params_w() +
			format("    --use_existing_db=true                        \\\n") +
			format("    --threads={}                                  \\\n", args->db_threads[number]) +
			format("    --readwritepercent={}                         \\\n", args->db_readwritepercent[number]) +
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

		string ret = get_docker_cmd() +
			format("  db_bench --benchmarks=mixgraph                  \\\n") +
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

		spdlog::info("Task {}, stdout: {}", name, alutils::str_replace(buffer, '\n', ' '));

		regex_search(buffer, cm, regex("thread ([0-9]+): \\(([0-9.]+),([0-9.]+)\\) ops and \\(([0-9.]+),([0-9.]+)\\) ops/second in \\(([0-9.]+),([0-9.]+)\\) seconds.*"), flags);
		if( cm.size() >= 8 ){
			ops += alutils::parseUint64(cm.str(2), true, 0, "invalid ops");
			ops_per_s += alutils::parseDouble(cm.str(4), true, 0, "invalid ops_per_s");
			data["ops"] = format("{}", ops);
			data["ops_per_s"] = format("{:.1f}", ops_per_s);
			data[format("ops[{}]", cm.str(1))] = cm.str(2);
			data[format("ops_per_s[{}]", cm.str(1))] = cm.str(4);
			//DEBUG_OUT("line parsed    : {}", buffer);
		}
		regex_search(buffer, cm, regex("Interval writes: ([0-9.]+[KMGT]*) writes, ([0-9.]+[KMGT]*) keys, ([0-9.]+[KMGT]*) commit groups, ([0-9.]+[KMGT]*) writes per commit group, ingest: ([0-9.]+) [KMGT]*B, ([0-9.]+) [KMGT]*B/s.*"), flags);
		if( cm.size() >= 7 ){
			data["writes"] = cm.str(1);
			data["written_keys"] = cm.str(2);
			data["written_commit_groups"] = cm.str(3);
			data["ingest_MBps"] = cm.str(5);
			data["ingest_MBps"] = cm.str(6);
			//DEBUG_OUT("line parsed    : {}", buffer);
		}
		regex_search(buffer, cm, regex("Interval WAL: ([0-9.]+[KMGT]*) writes, ([0-9.]+[KMGT]*) syncs, ([0-9.]+[KMGT]*) writes per sync, written: ([0-9.]+) [KMGT]*B, ([0-9.]+) [KMGT]*B/s.*"), flags);
		if( cm.size() >= 5 ){
			data["WAL_writes"] = cm.str(1);
			data["WAL_syncs"] = cm.str(2);
			data["WAL_written_MB"] = cm.str(4);
			data["WAL_written_MBps"] = cm.str(5);
			//DEBUG_OUT("line parsed    : {}", buffer);
		}
		regex_search(buffer, cm, regex("Interval stall: ([0-9:.]+) H:M:S, ([0-9.]+) percent.*"), flags);
		if( cm.size() >= 3 ){
			data["stall"] = cm.str(1);
			data["stall_percent"] = cm.str(2);
			//DEBUG_OUT("line parsed    : {}", buffer);

			print();
			ops = 0;
			ops_per_s = 0;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "YCSB::"

class YCSB : public ExperimentTask {
	Args* args;
	uint number;
	string container_name;

	unique_ptr<alutils::Socket> socket_client;
	nlohmann::ordered_json data2;

	string workload_docker;
	string workload_ycsb;

	public:    //------------------------------------------------------------------
	YCSB(Clock* clock_, Args* args_, uint number_) : ExperimentTask(format("ycsb[{}]", number_), clock_, args_->warm_period * 60), args(args_), number(number_) {
		DEBUG_MSG("constructor");
		container_name = format("ycsb_{}", number_);

		set_workload_params();
	}

	~YCSB() {
		DEBUG_MSG("destructor");
		try {
			alutils::command_output(format("docker rm -f {}", container_name).c_str());
			process.reset(nullptr);
		} catch (const std::exception& e) {
			spdlog::warn(e.what());
		}
	}

	void checkCreate() {
		if (args->ydb_create)
			createDB();
	}

	void start() {
		string cmd(get_cmd_run());
		spdlog::info("Executing {}. Command:\n{}", name, cmd);
		process.reset(new alutils::ProcessController(
			name.c_str(),
			cmd.c_str(),
			[this](const char* v){this->stdoutHandler(v);},
			[this](const char* v){this->default_stderr_handler(v);}
			));
	}

	private: //------------------------------------------------------------------
	void createDB() {
		string config;
		if (args->rocksdb_config_file.length() > 0)
			config = format("    -p rocksdb.optionsfile=\"/rocksdb.options\" \\\n");
		string cmd = get_docker_cmd(0) +
			format("  ycsb.sh load rocksdb -s                         \\\n") +
			get_const_params() +
			config +
			format("    2>&1 ");

		spdlog::info("Bulkload {}. Command:\n{}", name, cmd);
		auto ret = std::system(cmd.c_str());
		if (ret != 0)
			throw runtime_error(format("database bulkload error {}", ret).c_str());
	}

	string get_docker_cmd(uint32_t sleep) {
		string ret =
			format("docker run --name=\"{}\" -t --rm                  \\\n", container_name) +
			format("  --ulimit nofile=1048576:1048576                 \\\n") +
			format("  --user=\"{}\"                                   \\\n", getuid()) +
			format("  -v \"{}\":/workdata                             \\\n", args->ydb_path[number]) +
			format("  -v {}:/tmp/host                                 \\\n", tmpdir->getContainerDir(container_name).c_str());
		if (args->rocksdb_config_file.length() > 0) { ret +=
			format("  -v \"{}\":/rocksdb.options                      \\\n", tmpdir->getFileCopy(args->rocksdb_config_file).c_str());
		}
		ret += get_jni_param();
		ret += workload_docker;
		if (loglevel.level == LogLevel::LOG_DEBUG_OUT || loglevel.level == LogLevel::LOG_DEBUG) { ret +=
			format("  -e ROCKSDB_RCM_DEBUG=1                           \\\n");
		}
		if (args->ydb_socket) { ret +=
			format("  -e ROCKSDB_RCM_SOCKET=/tmp/host/rocksdb.sock     \\\n");
		}
		if (sleep > 0) { ret +=
			format("  -e YCSB_SLEEP={}m                               \\\n", sleep);
		}
		if (args->docker_params.length() > 0) { ret +=
			format("  {}                                              \\\n", args->docker_params);
		}
		ret +=
			format("  {}                                              \\\n", args->docker_image);
		return ret;
	}

	string get_jni_param() {
		string ret;
		if (args->ydb_rocksdb_jni != "") {
			std::filesystem::path p = args->ydb_rocksdb_jni;
			std::error_code ec;
			if (! std::filesystem::is_regular_file(p, ec) ) {
				throw runtime_error(format("parameter ydb_rocksdb_jni=\"{}\" is not a regular file: {}", args->ydb_rocksdb_jni, ec.message()).c_str());
			}
			ret += format("  -v {}:/opt/YCSB/rocksdb/target/dependency/rocksdbjni-linux64.jar:ro \\\n", std::filesystem::absolute(p).string());
		}
		return ret;
	}

	void set_workload_params() {
		if (std::filesystem::is_regular_file(args->ydb_workload[number])) {
			workload_docker = format("  -v {}:/ycsb_workloadfile                        \\\n", args->ydb_workload[number]);
			workload_ycsb   = "/ycsb_workloadfile";
		} else {
			workload_ycsb   = format("/opt/YCSB/workloads/{}", args->ydb_workload[number]);
		}
		DEBUG_MSG("workload_ycsb = {}", workload_ycsb);
	}

	string get_const_params() {
		string ret =
			format("    -P \"{}\"                                     \\\n", workload_ycsb) +
			format("    -p rocksdb.dir=\"/workdata\"                  \\\n") +
			format("    -p recordcount={}                             \\\n", args->ydb_num_keys[number]);
		return ret;
	}

	string get_cmd_run() {

		string cmd = get_docker_cmd(args->ydb_sleep[number]) +
			format("  ycsb.sh run rocksdb -s                          \\\n") +
			get_const_params() +
			format("    -p operationcount={}                          \\\n", 0) +
			format("    -p status.interval={}                         \\\n", args->stats_interval) +
			format("    -threads {}                                   \\\n", args->ydb_threads[number]);
			if (args->rocksdb_config_file.length() > 0) cmd +=
			format("    -p rocksdb.optionsfile=\"/rocksdb.options\"   \\\n");
			if (args->ydb_params[number].length() > 0) cmd +=
			format("    {}                                            \\\n", args->ydb_params[number]);
			cmd +=
			format("    2>&1 ");

		return cmd;
	}

	void stdoutHandler(const char* buffer) {
		const bool debug_handler = false;
		auto flags = std::regex_constants::match_any;
		std::cmatch cm;

		spdlog::info("Task {}, stdout: {}", name, alutils::str_replace(buffer, '\n', ' '));

		/* 2020-05-31 12:37:56:062 40 sec: 8898270 operations; 181027 current ops/sec; est completion in 5 second [READ: Count=452553, Max=2329, Min=1, Avg=19,59, 90=45, 99=69, 99.9=108, 99.99=602] [UPDATE: Count=452135, Max=404479, Min=5, Avg=87,65, 90=74, 99=1152, 99.9=1233, 99.99=2257] */

		regex_search(buffer, cm, regex("[0-9]{4}-[0-9]{2}-[0-9]{2} +[0-9:]+ +[0-9]+ +sec: +([0-9]+) +operations; +([0-9.,]+) +current[^\\[]+(.*)"));
		if (debug_handler) for (int i = 0; i < cm.size(); i++) {
			spdlog::info("text cm: {}", cm.str(i));
		}
		if (cm.size() >= 4) {
			string aux_replace;
			data["ops"] = cm.str(1);
			data["ops_per_s"] = alutils::str_replace(aux_replace, cm.str(2), ',', '.');

			string aux1 = cm.str(3);
			while (aux1.length() > 0) {
				regex_search(aux1.c_str(), cm, regex("\\[([^:]+): *([^\\]]+)\\] *(\\[.*)*"));
				if (cm.size() >=4) {
					if (debug_handler) for (int i = 0; i < cm.size(); i++) {
						spdlog::info("aux1 cm: {}", cm.str(i));
					}

					string prefix = cm.str(1);
					auto aux2 = alutils::split_str(cm.str(2), ", ");
					for (auto i: aux2) {
						auto aux3 = alutils::split_str(i, "=");
						if (aux3.size() >= 2){
							data[format("{}_{}", prefix, aux3[0])] = alutils::str_replace(aux_replace, aux3[1], ',', '.');
						}
					}

					aux1 = cm.str(3);
					if (debug_handler) spdlog::info("new aux1: {}", aux1);
				} else {
					break;
				}
			}

			if (args->ydb_socket) {
				try {
					if (socket_client.get() != nullptr && !socket_client->isActive()) {
						spdlog::error("socket client is not active for {}", name);
						socket_client.reset(nullptr);
					}
					if (socket_client.get() == nullptr) {
						auto socket_path = (tmpdir->getContainerDir(container_name) / "rocksdb.sock");
						spdlog::info("initiating socket client: {}", socket_path.string());
						socket_client.reset(new alutils::Socket(
								alutils::Socket::tClient,
								socket_path.string(),
								[this](alutils::Socket::HandlerData* data)->void{ socket_handler(data);},
								alutils::Socket::Params{.buffer_size=4096}
						));
					}

					data2 = get_data_and_clear();
					socket_client->send_msg("report column_family=usertable output=socket", true);
				} catch (std::exception& e) {
					spdlog::error("output handler exception from {} (socket client): {}", name, e.what());
				}

			} else {
				print();
			}
		}
	}

	void socket_handler(alutils::Socket::HandlerData* data) {
		try {
			DEBUG_MSG("msg = {}", data->msg);
			DEBUG_MSG("more_data = {}", data->more_data);

			auto flags = std::regex_constants::match_any;
			std::cmatch cm;
			std::regex_search(data->msg.c_str(), cm, std::regex("socket_server.json: (.*)"), flags);
			if (cm.size() >= 2) {
				DEBUG_MSG("add socket_report json to data2: {}", cm.str(1));
				data2["socket_report"] = nlohmann::ordered_json::parse(cm.str(1));
				print(data2);
			} else {
				spdlog::info("Task {}, socket output: {}", name, alutils::str_replace(data->msg, '\n', ' '));
			}
		} catch (std::exception& e) {
			spdlog::error("exception received in the socket handler of task {}: {}", name, e.what());
		}
	}

};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "AccessTime3::"

class AccessTime3 : public ExperimentTask {
	Args* args;
	uint number;
	string container_name;

	public:    //------------------------------------------------------------------
	AccessTime3(Clock* clock_, Args* args_, uint number_) : ExperimentTask(format("access_time3[{}]", number_), clock_, args_->warm_period * 60), args(args_), number(number_) {
		container_name = format("at3_{}", number_);
		DEBUG_MSG("constructor");
	}
	~AccessTime3() {
		DEBUG_MSG("destructor");
		try {
			alutils::command_output(format("docker rm -f {}", container_name).c_str());
			process.reset(nullptr);
		} catch (const std::exception& e) {
			spdlog::warn(e.what());
		}
	}

	void start() {
		string cmd( getCmd() );
		spdlog::info("Executing {}. Command:\n{}", name, cmd);
		process.reset(new alutils::ProcessController(
			name.c_str(),
			cmd.c_str(),
			[this](const char* v){this->stdoutHandler(v);},
			[this](const char* v){this->default_stderr_handler(v);}
			));
	}

	private:    //------------------------------------------------------------------
	string getCmd() {
		string ret;
		ret += format("docker run --name=\"{}\" -t --rm                  \\\n", container_name);
		ret += format("  --user=\"{}\"                                   \\\n", getuid());
		ret += format("  -v \"{}\":/workdata                             \\\n", args->at_dir[number]);
		ret += format("  -v {}:/tmp/host                                 \\\n", tmpdir->getContainerDir(container_name).c_str());
		ret += format("  {}                                              \\\n", args->docker_params);
		ret += format("  {}                                              \\\n", args->docker_image);
		ret +=        "  access_time3                                    \\\n" ;
		ret += format("    --duration={}                                 \\\n", args->duration * 60);
		ret += format("    --stats_interval={}                           \\\n", args->stats_interval);
		ret +=        "    --log_time_prefix=false                       \\\n";
		ret += format("    --filename=\"/workdata/{}\"                   \\\n", args->at_file[number]);
		ret +=        "    --create_file=false                           \\\n";
		ret += format("    --block_size={}                               \\\n", args->at_block_size[number]);
		ret += (args->at_io_engine[number].length() > 0) ?
		       format("    --io_engine=\"{}\"                            \\\n", args->at_io_engine[number]) : "";
		ret += (args->at_iodepth[number].length() > 0) ?
		       format("    --iodepth=\"{}\"                              \\\n", args->at_iodepth[number]) : "";
		ret += (args->at_o_direct[number].length() > 0) ?
		       format("    --o_direct=\"{}\"                             \\\n", args->at_o_direct[number]) : "";
		ret += (args->at_o_dsync[number].length() > 0) ?
		       format("    --o_dsync=\"{}\"                              \\\n", args->at_o_dsync[number]) : "";
		ret += format("    --command_script=\"{}\"                       \\\n", args->at_script[number]);
		ret += format("    {} 2>&1 ", args->at_params[number]);

		return ret;
	}

	void stdoutHandler(const char* buffer) {
		std::cmatch cm;

		spdlog::info("Task {}, stdout: {}", name, alutils::str_replace(buffer, '\n', ' '));

		regex_search(buffer, cm, regex("STATS: \\{[^,]+, ([^\\}]+)\\}"));
		if (cm.size() > 1) {
			auto clock_s = clock->s();
			if (clock_s > warm_period_s) {
				spdlog::info("Task {}, STATS: {} \"time\":\"{}\", {} {}", name, "{", clock_s - warm_period_s, cm.str(1), "}");
			}
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "PerformanceMonitorClient::"
// https://github.com/alange0001/performancemonitor

class PerformanceMonitorClient {
	Clock* clock;
	Args* args;
	unique_ptr<alutils::ThreadController> threadcontroller;
	uint64_t warm_period_s;

	int sock = 0;
	sockaddr_in serv_addr;

	public: //---------------------------------------------------------------------
	PerformanceMonitorClient(Clock* clock_, Args* args_) : clock(clock_), args(args_) {
		warm_period_s = args->warm_period * 60;

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			throw runtime_error("Socket creation error");
		}

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(args->perfmon_port);
		if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
			throw runtime_error("Invalid address / Address not supported");
		}

		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			throw runtime_error("Connection Failed. Performancemonitor is not running (https://github.com/alange0001/performancemonitor)");
		}
		DEBUG_MSG("socket fd={}", sock);

		threadcontroller.reset(
			new alutils::ThreadController(
				[this](alutils::ThreadController::stop_t stop){this->threadMain(stop);}
			)
		);
	}
	~PerformanceMonitorClient() {
		threadcontroller.reset(nullptr);
	}

	void stop() {threadcontroller->stop();}
	bool isActive(bool throw_exception=true) {return threadcontroller->isActive(throw_exception);}

	void threadMain(alutils::ThreadController::stop_t stop) {
		// based on: https://www.geeksforgeeks.org/socket-programming-cc/

		uint32_t buffer_size = 1024 * 1024;
		char buffer[buffer_size +1]; buffer[buffer_size] = '\0';
		std::cmatch cm;

		string send_msg = "reset";
		send(sock, send_msg.c_str(), send_msg.length(), 0);
		DEBUG_MSG("message \"{}\" sent", send_msg);

		send_msg = "stats";
		while (! stop()) {
			std::this_thread::sleep_for(seconds(args->stats_interval));

			send(sock, send_msg.c_str(), send_msg.length(), 0);
			DEBUG_MSG("message \"{}\" sent", send_msg);

			auto r = read(sock , buffer, buffer_size);
			if (r < 0) {
				close(sock);
				throw runtime_error(format("failed to read stats from performancemonitor (errno={})", errno));
			} else if (r == 0) {
				spdlog::warn("failed to read stats from performancemonitor (zero bytes received)");
				const string alive_msg( "alive" );
				send(sock, alive_msg.c_str(), alive_msg.length(), 0);
				DEBUG_MSG("message \"{}\" sent", alive_msg);
				r = read(sock , buffer, buffer_size);
				if (r <= 0) {
					close(sock);
					throw runtime_error(format("failed to read alive status from performancemonitor (errno={})", errno));
				}
				continue;
			}

			DEBUG_MSG("message received (size {})", r);
			assert(r <= buffer_size);
			buffer[r] = '\0';

			auto clock_s = clock->s();
			if (clock_s > warm_period_s) {
				regex_search(buffer, cm, regex("STATS: \\{(.+)"));
				if (cm.size() > 0) {
					spdlog::info("Task performancemonitor, STATS: {{\"time\": {}, {}", clock_s - warm_period_s, cm.str(1));
				}
			}
		}
		send(sock, "stop", sizeof("stop"), 0);
		DEBUG_MSG("close connection");
		close(sock);
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Program::"

#define ALL_Signals_F( _f ) \
		_f(SIGTERM) \
		_f(SIGINT)

class Program {
	static Program*   this_;
	unique_ptr<Args>  args;
	unique_ptr<Clock> clock;

	bool      is_reseting = false;
	int       ignore_signals = 10;
	const int ignore_signals_max = 10;

	unique_ptr<unique_ptr<DBBench>[]>     dbbench_list;
	unique_ptr<unique_ptr<YCSB>[]>        ycsb_list;
	unique_ptr<unique_ptr<AccessTime3>[]> at_list;
	unique_ptr<PerformanceMonitorClient>  perfmon;

	public: //---------------------------------------------------------------------
	Program() {
		DEBUG_MSG("constructor");
		system_check();
		
		Program::this_ = this;
#		define setSignalHandler(name) std::signal(name, Program::signalWrapper);
		ALL_Signals_F( setSignalHandler );
#		undef setSignalHandler
	}
	~Program() {
		DEBUG_MSG("destructor");
#		define unsetSignalHandler(name) std::signal(name, SIG_DFL);
		ALL_Signals_F( unsetSignalHandler );
#		undef unsetSignalHandler
		Program::this_ = nullptr;
	}

	int main(int argc, char** argv) noexcept {
		DEBUG_MSG("initialized");
		spdlog::info("rocksdb_test version: " ROCKSDB_TEST_VERSION);
		try {
			args.reset(new Args(argc, argv));
			clock.reset(new Clock());
			tmpdir.reset(new TmpDir());

			auto num_dbs = args->num_dbs;
			auto num_ydbs = args->num_ydbs;
			auto num_at  = args->num_at;
			if (num_dbs == 0 && num_ydbs == 0 && num_at == 0) {
				spdlog::warn("no benchmark specified");
				return 0;
			}

			// create DBBench instances and create DBs, if necessary
			dbbench_list.reset(new unique_ptr<DBBench>[num_dbs]);
			for (uint32_t i=0; i<num_dbs; i++) {
				dbbench_list[i].reset(new DBBench(clock.get(), args.get(), i));
				dbbench_list[i]->checkCreate();
			}
			// create YCSB instances and create DBs, if necessary
			ycsb_list.reset(new unique_ptr<YCSB>[num_ydbs]);
			for (uint32_t i=0; i<num_ydbs; i++) {
				ycsb_list[i].reset(new YCSB(clock.get(), args.get(), i));
				ycsb_list[i]->checkCreate();
			}

			clock->reset(); // reset clock
			uint64_t warm_period_s = 60 * args->warm_period;

			// start DBs
			for (uint32_t i=0; i<num_dbs; i++) {
				dbbench_list[i]->start();
			}
			for (uint32_t i=0; i<num_ydbs; i++) {
				ycsb_list[i]->start();
			}

			// create and start access_time3 instances
			at_list.reset(new unique_ptr<AccessTime3>[num_at]);
			for (uint32_t i=0; i<num_at; i++) {
				at_list[i].reset(new AccessTime3(clock.get(), args.get(), i));
				at_list[i]->start();
			}

			if (args->perfmon)
				perfmon.reset(new PerformanceMonitorClient(clock.get(), args.get()));

			bool stop = false;
			while ( !stop && clock->s() <= (args->duration * 60) )
			{
				// performancemonitor
				if (args->perfmon && (perfmon.get() == nullptr || ! perfmon->isActive())) {
					throw runtime_error("performancemonitor client is not active");
				}

				// db_bench
				for (uint32_t i=0; i<num_dbs; i++) {
					if (dbbench_list.get() == nullptr || dbbench_list[i].get() == nullptr || !dbbench_list[i]->isActive()) {
						stop = true;
						break;
					}
				}
				if (stop) break;

				// ycsb
				for (uint32_t i=0; i<num_ydbs; i++) {
					if (ycsb_list.get() == nullptr || ycsb_list[i].get() == nullptr || !ycsb_list[i]->isActive()) {
						stop = true;
						break;
					}
				}
				if (stop) break;

				// access_time3
				for (uint32_t i=0; i<num_at; i++) {
					if (at_list.get() == nullptr || at_list[i].get() == nullptr || !at_list[i]->isActive()) {
						stop = true;
						break;
					}
				}
				if (stop) break;

				std::this_thread::sleep_for(milliseconds(500));
			}

			spdlog::info("main loop finished");
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
		DEBUG_MSG("destroy tasks begin");
		if (! is_reseting) {
			is_reseting = true;
			ignore_signals = 0;

			dbbench_list.reset(nullptr);
			ycsb_list.reset(nullptr);
			at_list.reset(nullptr);
			perfmon.reset(nullptr);
			DEBUG_MSG("destroy tasks end");

			std::this_thread::sleep_for(milliseconds(1000));
			DEBUG_MSG("kill children begin");
			auto children = alutils::get_children(getpid(), true);
			for (auto i: children) {
				if (i != getpid()) {
					spdlog::warn("child (pid {}) still active. kill it", i);
					kill(i, SIGTERM);
				}
			}
			DEBUG_MSG("kill children end");

			tmpdir.reset(nullptr);

			ignore_signals = ignore_signals_max;
		}
	}

	static void signalWrapper(int signal) noexcept {
		if (Program::this_)
			Program::this_->signalHandler(signal);
	}
	void signalHandler(int signal) noexcept {
		spdlog::warn("received signal {}: {}", signal, signalName(signal));
		if (ignore_signals < ignore_signals_max) {
			spdlog::warn("signal ignored");
			ignore_signals++;
		} else {
			std::signal(signal, SIG_DFL);

			resetAll();
			kill(getpid(), signal);
		}
	}
	const char* signalName(int signal) const noexcept {
#		define sigReturn(name) if (signal == name) return #name;
		ALL_Signals_F( sigReturn );
#		undef sigReturn
		return "undefined";
	}
	
	void system_check() {
		if (! std::system(nullptr)) {
			fprintf(stderr, "ERROR: failed to initiate the command processor\n");
			exit(1);
		}
		if (std::system("docker ps -a >/dev/null")) {
			fprintf(stderr, "ERROR: failed to use docker command\n");
			exit(1);
		}
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
