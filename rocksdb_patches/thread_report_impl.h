// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#pragma once

#include "thread_report.h"

#include <alutils/print.h>
#include <alutils/string.h>
#include <alutils/socket.h>
#include <nlohmann/json.hpp>

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

#define TR_DEBUG(format, ...) if (env.debug) fprintf(stderr, "DEBUG ThreadReport::%s() [%d]: " format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define TR_DEBUG_E(format, ...) if (debug) fprintf(stderr, "DEBUG ThreadReport::%s() [%d]: " format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define TR_ERROR(format, ...) fprintf(stderr, "ThreadReport ERROR: " format "\n", ##__VA_ARGS__)
#define TR_PRINT(format, ...) fprintf(stderr, "ThreadReport: " format "\n", ##__VA_ARGS__)
#define TR_REPORT(format, ...) fprintf(stderr, "ThreadReport REPORT: " format "\n", ##__VA_ARGS__)

#define v2s(val) std::to_string(val).c_str()

namespace ROCKSDB_NAMESPACE {

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ThreadReportImpl::"

class ThreadReportImpl : public ThreadReport {
	struct Env {
		const char*  envname_debug    = "ROCKSDB_TR_DEBUG";
		const char*  envname_socket   = "ROCKSDB_TR_SOCKET";
		const char*  envname_interval              = "ROCKSDB_TR_INTERVAL";
		const char*  envname_interval_map          = "ROCKSDB_TR_INTERVAL_MAP";
		const char*  envname_interval_cfname       = "ROCKSDB_TR_INTERVAL_CFNAME";
		const char*  envname_interval_properties   = "ROCKSDB_TR_INTERVAL_PROPERTIES";
		const char*  envname_interval_cfproperties = "ROCKSDB_TR_INTERVAL_CFPROPERTIES";

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
			TR_DEBUG_E("debug = %s", debug?"true":"false");
			eaux = getenv(envname_socket);
			if (eaux != nullptr && eaux[0] != '\0') {
				socket = eaux;
			}
			TR_DEBUG_E("socket = %s", socket.c_str());
			eaux = getenv(envname_interval);
			if (eaux != nullptr) {
				auto aux2 = std::atoi(eaux);
				if (aux2 >=1)
					interval = aux2;
				else
					TR_ERROR("invalid value for the environment variable %s: %d", envname_interval, aux2);
			}
			TR_DEBUG_E("interval = %s", v2s(interval));
			eaux = getenv(envname_interval_map);
			if (eaux != nullptr && eaux[0] == '1') {
				interval_map = true;
			}
			TR_DEBUG_E("interval_map = %s", interval_map?"true":"false");
			eaux = getenv(envname_interval_cfname);
			if (eaux != nullptr && eaux[0] != '\0') {
				interval_cfname = eaux;
			}
			TR_DEBUG_E("interval_cfname = %s", interval_cfname.c_str());
			eaux = getenv(envname_interval_properties);
			if (eaux != nullptr && eaux[0] != '\0'){
				interval_properties = StringSplit(eaux, ',');
			}
			TR_DEBUG_E("interval_properties = %s", eaux);
			eaux = getenv(envname_interval_cfproperties);
			if (eaux != nullptr && eaux[0] != '\0'){
				interval_cfproperties = StringSplit(eaux, ',');
			}
			TR_DEBUG_E("interval_cfproperties = %s", eaux);
		}
	} env;

	DB* db;
	std::map<std::string, ColumnFamilyHandle*> cfmap;

	bool stop_ = false;
	bool active = false;

	std::unique_ptr<alutils::Socket> socket_server;

	public:
	ThreadReportImpl(): db(nullptr) {
		throw std::runtime_error("not implemented");
	}
	ThreadReportImpl(DB *db_, std::vector<ColumnFamilyHandle*>* handles) : db(db_) {
		TR_DEBUG("constructor begin");
		if (env.debug) {
			alutils::log_level = alutils::LOG_DEBUG;
		}

		if (handles != nullptr) {
			for (auto h : (*handles)){
				if (h != nullptr) {
					auto h_cfname = h->GetName();
					TR_PRINT("registering column family: %s", h_cfname.c_str());
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
			TR_PRINT("initiating report interval thread");
			std::thread thread = std::thread(&ThreadReportImpl::thread_main, this);
			thread.detach();
		}
		TR_DEBUG("constructor end");
	}
	~ThreadReportImpl() {
		stop_ = true;
		TR_DEBUG("destructor begin");

		socket_server.reset(nullptr);

		for (int i=0; i < 20 && active; i++){
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		TR_DEBUG("destructor end");
	}

	private:
	void thread_main() noexcept {
		active = true;
		try {
			ColumnFamilyHandle* cfhandle = nullptr;
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
						TR_REPORT("");
						TR_REPORT("==========================================");
						TR_REPORT("BEGIN %s:", s.c_str());
						if (env.interval_map) {
							std::map<std::string, std::string> mstats;
							if (db->GetMapProperty(s.c_str(), &mstats)) {
								for (auto const& ist : mstats) {
									TR_REPORT("%s :\t%s", ist.first.c_str(), ist.second.c_str());
								}
							}
						} else {
							if (db->GetProperty(s.c_str(), &stats)) {
								std::stringstream stats_stream(stats);
								std::string line;
								while( std::getline(stats_stream, line) ) {
									TR_REPORT("%s", line.c_str());
								}
							}
						}
						TR_REPORT("END %s:", s.c_str());
					}
					if (stop_) break;

					if (env.interval_cfname.length() > 0) {
						for (auto s : env.interval_cfproperties) {
							TR_REPORT("");
							TR_REPORT("==========================================");
							TR_REPORT("BEGIN %s, COLUMN FAMILY %s:", s.c_str(), cfhandle->GetName().c_str());
							if (env.interval_map) {
								std::map<std::string, std::string> mstats;
								if (db->GetMapProperty(cfhandle, s.c_str(), &mstats)) {
									for (auto const& ist : mstats) {
										TR_REPORT("%s :\t%s", ist.first.c_str(), ist.second.c_str());
									}
								}
							} else {
								std::string stats;
								if (db->GetProperty(cfhandle, s.c_str(), &stats)) {
									std::stringstream stats_stream(stats);
									std::string line;
									while( std::getline(stats_stream, line) ) {
										TR_REPORT("%s", line.c_str());
									}
								}
							}
							TR_REPORT("END %s, COLUMN FAMILY %s:", s.c_str(), cfhandle->GetName().c_str());
						}
					}
					if (stop_) break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		} catch (const std::exception& e) {
			TR_ERROR("report interval exception: %s", e.what());
		}
		TR_PRINT("report interval thread finished");
		active = false;
	}

	enum type_t { tSocket, tStderr };
	type_t getType(const std::string& str) {
		if (str == "socket")
			return tSocket;
		if (str == "stderr" || str == "")
			return tStderr;
		std::string errmsg("invalid output type: "); errmsg += str;
		TR_ERROR("%s", errmsg.c_str());
		return tStderr;
	}

	void socket_handler(alutils::Socket::HandlerData* data) {
		TR_DEBUG("message received: %s", data->msg.c_str());

		if (alutils::ParseRE(data->msg, "^report( .*){0,1}").valid) {
			nlohmann::ordered_json rep;
			std::string aux;
			std::map<std::string, std::string> mstats;
			std::string stats_name = "rocksdb.cfstats";

			std::string column_family = alutils::ParseRE(data->msg, " column_family *= *([\\w]+)", aux).valid ? aux : "";
			type_t type = alutils::ParseRE(data->msg, " output *= *([\\w]+)", aux).valid ? getType(aux) : tStderr;

			if (column_family == "") {
				TR_DEBUG("reading database statistics: %s", stats_name.c_str());
				if (! db->GetMapProperty(stats_name.c_str(), &mstats)) {
					TR_ERROR("failed to retrieve %s", stats_name.c_str());
					return;
				}
				rep[stats_name] = mstats;

			} else {
				ColumnFamilyHandle* cfhandle = nullptr;
				if (cfmap.count(column_family) > 0) {
					cfhandle = cfmap[column_family];
				} else {
					TR_ERROR("column_family=\"%s\" not found", column_family.c_str());
					return;
				}
				rep["column_family"] = column_family;
				TR_DEBUG("reading database statistics: %s, column_family=%s", stats_name.c_str(), column_family.c_str());
				if (! db->GetMapProperty(cfhandle, stats_name.c_str(), &mstats)) {
					TR_ERROR("failed to retrieve %s from column_family=%s", stats_name.c_str(), column_family.c_str());
					return;
				}
				rep[stats_name] = mstats;
			}

			TR_DEBUG("reporting stats");
			if (type == tStderr) {
				TR_REPORT("socket_server.json: %s", rep.dump().c_str());
			} else {
				data->send(std::string("socket_server.json: ") + rep.dump() + "\n", false);
			}
		} else if (alutils::ParseRE(data->msg, "^test( .*){0,1}").valid) {
			TR_REPORT("test response: OK!");
			data->send("test response: OK!\n", false);
		} else {
			TR_ERROR("invalid socket command: %s", data->msg.c_str());
		}
	}
};

} // namespace ROCKSDB_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////

#define THREADREPORT_OPEN                                                                                \
	auto s = DBImpl::Open(db_options, dbname, column_families, handles, dbptr,                           \
						!kSeqPerBatch, kBatchPerTxn);                                                    \
	if (s.ok() && *dbptr != nullptr){                                                                    \
	  (*dbptr)->thread_report.reset(static_cast<ThreadReport*>(new ThreadReportImpl(*dbptr, handles)));  \
	}                                                                                                    \
	return s

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""
