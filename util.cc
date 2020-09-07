// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#include "util.h"

#include <stdexcept>
#include <regex>
#include <limits>

#include <alutils/string.h>
#include <alutils/process.h>

using std::string;
using std::vector;
using std::function;
using std::exception;
using std::invalid_argument;
using std::regex_search;
using std::regex;
using std::runtime_error;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using fmt::format;

////////////////////////////////////////////////////////////////////////////////////
using alutils::vsprintf;
ALUTILS_PRINT_WRAPPER(alutils_print_debug,    spdlog::debug("{}", msg));
ALUTILS_PRINT_WRAPPER(alutils_print_info,     spdlog::info("{}", msg));
ALUTILS_PRINT_WRAPPER(alutils_print_warn,     spdlog::warn("{}", msg));
ALUTILS_PRINT_WRAPPER(alutils_print_error,    spdlog::error("{}", msg));
ALUTILS_PRINT_WRAPPER(alutils_print_critical, spdlog::critical("{}", msg));

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "LogLevel::"

LogLevel loglevel;

LogLevel::LogLevel() {
	alutils::print_debug_out = alutils_print_debug;
	alutils::print_debug     = alutils_print_debug;
	alutils::print_info      = alutils_print_info;
	alutils::print_notice    = alutils_print_info;
	alutils::print_warn      = alutils_print_warn;
	alutils::print_error     = alutils_print_error;
	alutils::print_critical  = alutils_print_critical;

	set("info");
}

void LogLevel::set(const string& name) {
	for (int i=0; map_names[i] != NULL; i++) {
		if (name == map_names[i]) {
			level = (levels)i;
			alutils::log_level = map_alutils[i];
			spdlog::set_level(map_spdlog[i]);
			DEBUG_MSG("set log level to {}", name);
			return;
		}
	}
	string aux = format("invalid log level: {}. Possible values: {}", name, map_names[0]);
	for (int i=1; map_names[i] != NULL; i++)
		aux += string(", ") + map_names[i];
	throw invalid_argument(aux);
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "SystemStat::CPUCounters::"

SystemStat::CPUCounters::CPUCounters(const string& src) {
	auto aux = alutils::split_str(src, " ");
	for (int i = 0; i<aux.size(); i++) {
		auto value = alutils::parseUint64(aux[i], true, 0);
		data.push_back( value );

		total += value;
		if (i == S_IDLE || i == S_IOWAIT || i == S_STEAL) {
			idle += value;
		} else {
			active += value;
		}
	}
	if (data.size() < NUM_COUNTERS)
		throw runtime_error("error parsing /proc/stat");
	//DEBUG_MSG("total={}, active={}, idle={}", total, active, idle);
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "SystemStat::"

SystemStat::SystemStat() {
	DEBUG_MSG("constructor");

	getLoad();
	getCPU();
}

SystemStat::~SystemStat() {}

string SystemStat::json(const SystemStat& prev, const string& first_attributes) {
#	define addAttr(name, format_) ret += format("{}\"" #name "\":\"{" format_ "}\"", (ret.length()>0)?", ":"", name)
#	define addAttr2(name, value, format_) ret += format("{}\"{}\":\"{" format_ "}\"", (ret.length()>0)?", ":"", name, value)
	string ret = first_attributes;

	/////////// LOAD //////////////

	addAttr(load1, ":.1f");
	addAttr(load5, ":.1f");
	addAttr(load15, ":.1f");

	/////////// CPU /////////////
	auto addCPU = [&](const string& prefix, const CPUCounters& c_cur, const CPUCounters& c_prev) {
		addAttr2(format("{}.active", prefix), c_cur.getActive(c_prev), ":.2f");
		addAttr2(format("{}.user", prefix), c_cur.getByType(c_prev, CPUCounters::S_USER), ":.2f");
		addAttr2(format("{}.system", prefix), c_cur.getByType(c_prev, CPUCounters::S_SYSTEM), ":.2f");
		addAttr2(format("{}.iowait", prefix), c_cur.getByType(c_prev, CPUCounters::S_IOWAIT), ":.2f");
	};
	addCPU("cpus", cpu_all, prev.cpu_all);

	for (int i=0; i<cpu.size(); i++) {
		addCPU(format("cpu[{}]", i), cpu[i], prev.cpu[i]);
	}

	return format("{} {} {}", "{", ret, "}");
#	undef addAttr
#	undef addAttr2
}

void SystemStat::getLoad() {
	string out = alutils::command_output("uptime");

	std::cmatch cm;
	regex_search(out.c_str(), cm, regex("load average:\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+)"));
	if( cm.size() >= 4 ){
		string aux;
		alutils::str_replace(aux, cm.str(1), ',', '.');
		load1 = alutils::parseDouble(aux, true, 0);
		alutils::str_replace(aux, cm.str(2), ',', '.');
		load5 = alutils::parseDouble(aux, true, 0);
		alutils::str_replace(aux, cm.str(3), ',', '.');
		load15 = alutils::parseDouble(aux, true, 0);
	} else {
		throw runtime_error("unable to parse the output of uptime");
	}
}

void SystemStat::getCPU() {
	string out = alutils::command_output("cat /proc/stat");
	auto flags = std::regex_constants::match_any;
	std::cmatch cm;

	regex_search(out.c_str(), cm, regex("cpu +(.*)"), flags);
	if (cm.size() < 2)
		throw runtime_error("no cpu line in the file /proc/stat");
	cpu_all = cm.str(1);

	for (uint32_t i=0; i<1024; i++) {
		regex_search(out.c_str(), cm, regex(format("cpu{} +(.*)", i)), flags);
		if (cm.size() >=2 ) {
			cpu.push_back(cm.str(1));
		} else {
			break;
		}
	}
}
