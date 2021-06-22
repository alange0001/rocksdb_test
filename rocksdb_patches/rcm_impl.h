// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#pragma once

#include "rcm.h"

#include <alutils/print.h>
#include <alutils/string.h>
#include <alutils/socket.h>
#include <nlohmann/json.hpp>

#include <sstream>

namespace RCM {

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

#define RCM_PRINT_F(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)
#define RCM_DEBUG_CONDITION debug

#define RCM_DEBUG(format, ...) if (RCM_DEBUG_CONDITION) {RCM_PRINT_F("DEBUG %s%s() [%d]: " format, __CLASS__, __func__, __LINE__, ##__VA_ARGS__);}
#define RCM_ERROR(format, ...) RCM_PRINT_F("RCM ERROR: " format, ##__VA_ARGS__)
#define RCM_PRINT(format, ...) RCM_PRINT_F("RCM: " format, ##__VA_ARGS__)
#define RCM_REPORT(format, ...) RCM_PRINT_F("RCM REPORT: " format, ##__VA_ARGS__)

#define RCM_DEBUG_SM(...)

#define v2s(val) std::to_string(val).c_str()

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "RCM::Env::"

struct Env {
	const char*  envname_debug    = "ROCKSDB_RCM_DEBUG";
	const char*  envname_socket   = "ROCKSDB_RCM_SOCKET";
	const char*  envname_interval              = "ROCKSDB_RCM_INTERVAL";
	const char*  envname_interval_map          = "ROCKSDB_RCM_INTERVAL_MAP";
	const char*  envname_interval_cfname       = "ROCKSDB_RCM_INTERVAL_CFNAME";
	const char*  envname_interval_properties   = "ROCKSDB_RCM_INTERVAL_PROPERTIES";
	const char*  envname_interval_cfproperties = "ROCKSDB_RCM_INTERVAL_CFPROPERTIES";

	bool         debug = false;
	std::string  socket = "";
	int          interval = 0;
	std::string  interval_cfname = "";
	bool         interval_map = false;
	std::vector<std::string> interval_properties;
	std::vector<std::string> interval_cfproperties;

	Env() {
		const char* eaux;
		eaux = getenv(envname_debug);
		if (eaux != nullptr && eaux[0] == '1') {
			debug = true;
		}
		RCM_DEBUG("debug = %s", debug?"true":"false");
		eaux = getenv(envname_socket);
		if (eaux != nullptr && eaux[0] != '\0') {
			socket = eaux;
		}
		RCM_DEBUG("socket = %s", socket.c_str());
		eaux = getenv(envname_interval);
		if (eaux != nullptr) {
			auto aux2 = std::atoi(eaux);
			if (aux2 >=1)
				interval = aux2;
			else
				RCM_ERROR("invalid value for the environment variable %s: %d", envname_interval, aux2);
		}
		RCM_DEBUG("interval = %s", v2s(interval));
		eaux = getenv(envname_interval_map);
		if (eaux != nullptr && eaux[0] == '1') {
			interval_map = true;
		}
		RCM_DEBUG("interval_map = %s", interval_map?"true":"false");
		eaux = getenv(envname_interval_cfname);
		if (eaux != nullptr && eaux[0] != '\0') {
			interval_cfname = eaux;
		}
		RCM_DEBUG("interval_cfname = %s", interval_cfname.c_str());
		eaux = getenv(envname_interval_properties);
		if (eaux != nullptr && eaux[0] != '\0'){
			interval_properties = ROCKSDB_NAMESPACE::StringSplit(eaux, ',');
		}
		RCM_DEBUG("interval_properties = %s", eaux);
		eaux = getenv(envname_interval_cfproperties);
		if (eaux != nullptr && eaux[0] != '\0'){
			interval_cfproperties = ROCKSDB_NAMESPACE::StringSplit(eaux, ',');
		}
		RCM_DEBUG("interval_cfproperties = %s", eaux);
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "RCM::CommandLine::"

struct CommandLine {
	bool debug = false;
	bool valid = false;
	std::string command;
	std::string params_str;
	std::map<std::string, std::string> params;

	CommandLine(Env& env, const std::string& line) : debug(env.debug) {
		std::smatch sm;

		std::regex_search(line, sm, std::regex("^(.+)"));
		RCM_DEBUG_SM(sm);
		if (sm.size() == 0) return;
		std::string line_aux = sm.str(0);
		if (line_aux.length() == 0) return;

		std::regex_search(line_aux, sm, std::regex("^(\\w+)(|\\s+.+)$"));
		RCM_DEBUG_SM(sm);
		if (sm.size() < 3) return;
		command = sm.str(1);
		params_str = sm.str(2);
		std::string tail = params_str;

		while (true) {
			std::regex_search(tail, sm, std::regex("(\\w+)\\s*=\\s*'([^']+)'"));
			RCM_DEBUG_SM(sm);
			if (sm.size() >= 3) {
				params[sm.str(1)] = sm.str(2);
				tail.replace(tail.find(sm.str(0)), sm.str(0).length(), "");
				continue;
			}
			std::regex_search(tail, sm, std::regex("(\\w+)\\s*=\\s*\"([^\"]+)\""));
			RCM_DEBUG_SM(sm);
			if (sm.size() >= 3) {
				params[sm.str(1)] = sm.str(2);
				tail.replace(tail.find(sm.str(0)), sm.str(0).length(), "");
				continue;
			}
			std::regex_search(tail, sm, std::regex("(\\w+)\\s*=\\s*(\\w+)"));
			RCM_DEBUG_SM(sm);
			if (sm.size() >= 3) {
				params[sm.str(1)] = sm.str(2);
				tail.replace(tail.find(sm.str(0)), sm.str(0).length(), "");
				continue;
			}
			break;
		}

		valid = true;
	}

	bool operator== (bool val) {
		return val == valid;
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "RCM::OutputHandler::"

class OutputHandler {
	bool output_socket = false;
	bool output_stderr = true;
	alutils::Socket::HandlerData* data = nullptr;

	public: //-----------------------------------------------------------------
	bool debug = false;

	OutputHandler() {}
	OutputHandler(Env& env) : debug(env.debug) {}

	OutputHandler(Env& env, alutils::Socket::HandlerData* data_, CommandLine& cmd) : data(data_) {
		debug = env.debug;
		output_socket = true;
		output_stderr = false;
		if (cmd.params.count("output") > 0) {
			output_socket = (cmd.params["output"] == "socket");
			output_stderr = (cmd.params["output"] == "stderr");
			if (!output_socket && !output_stderr) {
				output_socket = true;
			}
		}
		if (cmd.params.count("debug") > 0) {
			if (cmd.params["debug"] == "1")
				debug = true;
			if (cmd.params["debug"] == "0")
				debug = false;
		}
		RCM_DEBUG("debug = %s, output_socket = %s, output_stderr = %s", v2s(debug), v2s(output_socket), v2s(output_stderr));
	}

	void print(const char* format, ...) {
		va_list args;
		va_start(args, format);
		std::string msg = alutils::vsprintf(format, args);
		va_end(args);
		if (output_socket && data != nullptr) {
			data->send((msg + "\n").c_str(), false);
		}
		if (output_stderr) {
			fprintf(stderr, "%s\n", msg.c_str());
		}
	}
};

#undef RCM_PRINT_F
#define RCM_PRINT_F(format, ...) output.print(format, ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "RCM::Controller::"

class ControllerImpl : public Controller {
	RCM::Env env;
	RCM::OutputHandler output;
	bool     debug  = false;
	bool     stop_  = false;
	bool     active = false;
	ROCKSDB_NAMESPACE::DB*      db;
	std::map<std::string, ROCKSDB_NAMESPACE::ColumnFamilyHandle*> cfmap;
	std::unique_ptr<alutils::Socket>           socket_server;

	public: //-----------------------------------------------------------------

	ControllerImpl(): db(nullptr) {
		throw std::runtime_error("invalid constructor");
	}

	ControllerImpl(ROCKSDB_NAMESPACE::DB *db_, std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>* handles) : db(db_) {
		debug = env.debug;
		output = RCM::OutputHandler(env);
		if (debug) {
			alutils::log_level = alutils::LOG_DEBUG;
		}
		RCM_DEBUG("constructor begin");

		if (handles != nullptr) {
			for (auto h : (*handles)){
				if (h != nullptr) {
					auto h_cfname = h->GetName();
					RCM_PRINT("registering column family: %s", h_cfname.c_str());
					cfmap[h_cfname] = h;
				}
			}
		}

		if (env.socket.length() > 0) {
			auto handler_l = [this](alutils::Socket::HandlerData* data)->void{socket_handler(data);};
			alutils::Socket::Params p; p.buffer_size=4096;
			socket_server.reset(new alutils::Socket(
					alutils::Socket::tServer,
					env.socket.c_str(),
					handler_l,
					p
			));
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		if (env.interval > 0) {
			RCM_PRINT("initiating report interval thread");
			std::thread thread = std::thread(&ControllerImpl::thread_main, this);
			thread.detach();
		}
		RCM_DEBUG("constructor end");
	}

	~ControllerImpl() {
		stop_ = true;
		RCM_DEBUG("destructor begin");

		socket_server.reset(nullptr);

		for (int i=0; i < 20 && active; i++){
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		RCM_DEBUG("destructor end");
	}

	private: //----------------------------------------------------------------

	void thread_main() noexcept {
		active = true;
		try {
			ROCKSDB_NAMESPACE::ColumnFamilyHandle* cfhandle = nullptr;
			if (env.interval_cfname != "") {
				if (cfmap.count(env.interval_cfname) > 0) {
					cfhandle = cfmap[env.interval_cfname];
				} else {
					env.interval_cfname = "";
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(200));

			int c = 0;
			while (! stop_) {
				c++;
				if (c > env.interval*5) {
					c = 0;
					for (auto s : env.interval_properties) {
						std::string stats;
						RCM_REPORT("");
						RCM_REPORT("==========================================");
						RCM_REPORT("BEGIN %s:", s.c_str());
						if (env.interval_map) {
							std::map<std::string, std::string> mstats;
							if (db->GetMapProperty(s.c_str(), &mstats)) {
								for (auto const& ist : mstats) {
									RCM_REPORT("%s :\t%s", ist.first.c_str(), ist.second.c_str());
								}
							}
						} else {
							if (db->GetProperty(s.c_str(), &stats)) {
								std::stringstream stats_stream(stats);
								std::string line;
								while( std::getline(stats_stream, line) ) {
									RCM_REPORT("%s", line.c_str());
								}
							}
						}
						RCM_REPORT("END %s:", s.c_str());
					}
					if (stop_) break;

					if (env.interval_cfname.length() > 0) {
						for (auto s : env.interval_cfproperties) {
							RCM_REPORT("");
							RCM_REPORT("==========================================");
							RCM_REPORT("BEGIN %s, COLUMN FAMILY %s:", s.c_str(), cfhandle->GetName().c_str());
							if (env.interval_map) {
								std::map<std::string, std::string> mstats;
								if (db->GetMapProperty(cfhandle, s.c_str(), &mstats)) {
									for (auto const& ist : mstats) {
										RCM_REPORT("%s :\t%s", ist.first.c_str(), ist.second.c_str());
									}
								}
							} else {
								std::string stats;
								if (db->GetProperty(cfhandle, s.c_str(), &stats)) {
									std::stringstream stats_stream(stats);
									std::string line;
									while( std::getline(stats_stream, line) ) {
										RCM_REPORT("%s", line.c_str());
									}
								}
							}
							RCM_REPORT("END %s, COLUMN FAMILY %s:", s.c_str(), cfhandle->GetName().c_str());
						}
					}
					if (stop_) break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		} catch (const std::exception& e) {
			RCM_ERROR("report interval exception: %s", e.what());
		}
		RCM_PRINT("report interval thread finished");
		active = false;
	}

#	undef  RCM_PRINT_F
#	define RCM_PRINT_F(format, ...) output2.print(format, ##__VA_ARGS__)
#	undef  RCM_DEBUG_CONDITION
#	define RCM_DEBUG_CONDITION output2.debug

	void socket_handler(alutils::Socket::HandlerData* data) {
		CommandLine cmd(env, data->msg);
		OutputHandler output2(env, data, cmd);
		RCM_DEBUG("message received: %s", data->msg.c_str());

		if (cmd.valid) {
			if (handle_report(cmd, output2)) return;
			if (handle_metadata(cmd, output2)) return;
			if (handle_compact_level(cmd, output2)) return;
			if (handle_test(cmd, output2)) return;
		}

		RCM_ERROR("invalid socket command: %s", data->msg.c_str());
	}

	bool handle_report(CommandLine& cmd, OutputHandler& output2) {
		if (cmd.command != "report")
			return false;

		nlohmann::ordered_json rep;
		std::string aux;
		std::map<std::string, std::string> mstats;
		std::string stats_name = "rocksdb.cfstats";

		std::string column_family = cmd.params["column_family"];

		if (column_family == "") {
			RCM_DEBUG("reading database statistics: %s", stats_name.c_str());
			if (! db->GetMapProperty(stats_name.c_str(), &mstats)) {
				RCM_ERROR("failed to retrieve %s", stats_name.c_str());
				return true;
			}
			rep[stats_name] = mstats;

		} else {
			ROCKSDB_NAMESPACE::ColumnFamilyHandle* cfhandle = nullptr;
			if (cfmap.count(column_family) > 0) {
				cfhandle = cfmap[column_family];
			} else {
				RCM_ERROR("column_family=\"%s\" not found", column_family.c_str());
				return true;
			}
			rep["column_family"] = column_family;
			RCM_DEBUG("reading database statistics: %s, column_family=%s", stats_name.c_str(), column_family.c_str());
			if (! db->GetMapProperty(cfhandle, stats_name.c_str(), &mstats)) {
				RCM_ERROR("failed to retrieve %s from column_family=%s", stats_name.c_str(), column_family.c_str());
				return true;
			}
			rep[stats_name] = mstats;
		}

		RCM_DEBUG("reporting stats");
		RCM_REPORT("socket_server.json: %s", rep.dump().c_str());
		return true;
	}

	bool handle_metadata(CommandLine& cmd, OutputHandler& output2) {
		if (cmd.command != "metadata")
			return false;

		std::string cfname(cmd.params["column_family"]);
		ROCKSDB_NAMESPACE::ColumnFamilyHandle* cfhandle = nullptr;
		if (cfname.length() == 0) {
			cfhandle = db->DefaultColumnFamily();
		} else if (cfmap.count(cfname) > 0) {
			cfhandle = cfmap[cfname];
		}

		nlohmann::ordered_json json;

		if (cfhandle != nullptr) {
			ROCKSDB_NAMESPACE::ColumnFamilyMetaData metadata;
			db->GetColumnFamilyMetaData(cfhandle, &metadata);
			json["name"] = metadata.name;
			json["size"] = metadata.size;
			json["file_count"] = metadata.file_count;
			for (auto &l: metadata.levels) {
				std::string level_prefix = alutils::sprintf("level%s.", v2s(l.level));
				json[level_prefix + "size"] = l.size;
				json[level_prefix + "file_count"] = l.files.size();

#				define FileAttrs( _f )                     \
					_f(name, std::string);                 \
					_f(size, std::to_string);              \
					_f(num_reads_sampled, std::to_string); \
					_f(num_entries, std::to_string);       \
					_f(num_deletions, std::to_string);     \
					_f(being_compacted, std::to_string)
#				define f_declare(a_name, ...) \
					std::string file_##a_name
#				define f_append(a_name, convf) \
					file_##a_name += (file_##a_name.length() > 0) ? ", " : ""; \
					file_##a_name += convf(f.a_name)
#				define f_add(a_name, ...) \
					json[level_prefix + "files." #a_name] = file_##a_name

				FileAttrs(f_declare);
				for (auto &f: l.files) {
					FileAttrs(f_append);
				}
				FileAttrs(f_add);

#				undef FileAttrs
#				undef f_declare
#				undef f_append
#				undef f_add
			}
		} else {
			RCM_ERROR("column family '%s' not found", cfname.c_str());
			return true;
		}

		RCM_REPORT("Column family metadata: %s", json.dump().c_str());
		return true;
	}

	bool handle_compact_level(CommandLine& cmd, OutputHandler& output2) {
		if (cmd.command != "compact_level")
			return false;

		ROCKSDB_NAMESPACE::ColumnFamilyHandle* cfhandle = db->DefaultColumnFamily();
		std::string cfname(cmd.params["column_family"]);
		if (cfname != "") {
			if (cfmap.count(cfname) > 0) {
				cfhandle = cfmap[cfname];
			} else {
				RCM_ERROR("invalid column family: %s", cfname.c_str());
				return true;
			}
		}
		ROCKSDB_NAMESPACE::ColumnFamilyMetaData metadata;
		db->GetColumnFamilyMetaData(cfhandle, &metadata);

		std::vector<int>::size_type level = 1;
		if (cmd.params.count("level") > 0) {
			std::istringstream auxs(cmd.params["level"]);
			auxs >> level;
		}
		if (level >= metadata.levels.size()) {
			RCM_ERROR("invalid level: %d", level);
			return true;
		}

		std::vector<int>::size_type target_level = level + 1;
		if (cmd.params.count("target_level") > 0) {
			std::istringstream auxs(cmd.params["target_level"]);
			auxs >> target_level;
		}
		if (target_level >= metadata.levels.size()) {
			RCM_ERROR("invalid target_level: %d");
			return true;
		}

		auto &l = metadata.levels[level];
		std::vector<int>::size_type files = 0;
		if (cmd.params.count("files") > 0) {
			std::istringstream auxs(cmd.params["files"]);
			auxs >> files;
		}
		if (files == 0 || files > l.files.size()) {
			files = l.files.size();
		}

		std::vector<std::string> input_file_names;
		std::vector<int>::size_type c = 0;
		for (auto& f: l.files) {
			if (c++ >= files) break;
			input_file_names.push_back(f.name);
		}

		{
			RCM_PRINT("Column Family %s: compacting %s files of %s from level %s", cfname.c_str(), v2s(files), v2s(l.files.size()), v2s(level));
			ROCKSDB_NAMESPACE::CompactionOptions compact_options;
			auto s = db->CompactFiles(compact_options, cfhandle, input_file_names, target_level);
			if (s.ok()) {
				RCM_PRINT("done!");
			} else {
				RCM_ERROR("failed!");
			}
		}

		return true;
	}

	bool handle_test(CommandLine& cmd, OutputHandler& output2) {
		if (cmd.command != "test")
			return false;

		RCM_REPORT("test response: OK!");
		return true;
	}
};

} // namespace RCM

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#undef v2s

////////////////////////////////////////////////////////////////////////////////////
#define RCM_OPEN_CMD                                                                                \
	auto s = DBImpl::Open(db_options, dbname, column_families, handles, dbptr,                           \
						!kSeqPerBatch, kBatchPerTxn);                                                    \
	if (s.ok() && *dbptr != nullptr){                                                                    \
	  (*dbptr)->rcm_controller.reset(static_cast<RCM::Controller*>(new RCM::ControllerImpl(*dbptr, handles)));  \
	}                                                                                                    \
	return s
