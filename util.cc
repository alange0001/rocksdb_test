
#include "util.h"

#include <stdexcept>
#include <regex>
#include <limits>

#include "process.h"

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
#undef __CLASS__
#define __CLASS__ ""

string strip(const string& src) {
	string ret = src;
	return inplace_strip(ret);
}

int split_columns(vector<string>& ret, const char* str, const char* prefix) {
	std::cmatch cm;
	auto flags = std::regex_constants::match_any;
	string str_aux;

	ret.clear();

	if (prefix != nullptr) {
		std::regex_search(str, cm, std::regex(fmt::format("{}\\s+(.+)", prefix).c_str()), flags);
		if (cm.size() < 2)
			return 0;

		str_aux = cm[1].str();
		str = str_aux.c_str();
	}

	for (const char* i = str;;) {
		std::regex_search(i, cm, std::regex("([^\\s]+)\\s*(.*)"), flags);
		if (cm.size() >= 3) {
			ret.push_back(cm[1].str());
			i = cm[2].first;
		} else {
			break;
		}
	}

	return ret.size();
}

vector<string> split_str(const string& str, const string& delimiter) {
	vector<string> ret;
	string aux = str;
	auto pos = string::npos;

	while ((pos = aux.find(delimiter)) != string::npos) {
		ret.push_back(strip(aux.substr(0, pos)));
		aux.erase(0, pos + delimiter.length());
	}
	ret.push_back(strip(aux));

	return ret;
}

bool parseBool(const string &value, const bool required, const bool default_,
               const char* error_msg,
			   function<bool(bool)> check_method )
{
	const char* true_str[] = {"y","yes","t","true","1", ""};
	const char* false_str[] = {"n","no","f","false","0", ""};
	bool set = (!required && value == "");
	bool ret = default_;

	if (!set) {
		for (const char** i = true_str; **i != '\0'; i++) {
			if (value == *i) {
				ret = true; set = true;
			}
		}
	}
	if (!set) {
		for (const char** i = false_str; **i != '\0'; i++) {
			if (value == *i) {
				ret = false; set = true;
			}
		}
	}

	if (!set)
		throw invalid_argument(error_msg);
	if (check_method != nullptr && !check_method(ret))
		throw invalid_argument(error_msg);

	return ret;
}

uint32_t parseUint32(const string &value, const bool required, const uint32_t default_,
               const char* error_msg,
			   function<bool(uint32_t)> check_method )
{
	if (required && value == "")
		throw invalid_argument(error_msg);
	uint32_t ret = default_;
	try {
		if (value != "")
			ret = std::stoul(value);
	} catch (exception& e) {
		throw invalid_argument(error_msg);
	}
	if (check_method != nullptr && !check_method(ret))
		throw invalid_argument(error_msg);
	return ret;
}

uint64_t parseUint64(const string &value, const bool required, const uint64_t default_,
               const char* error_msg,
			   function<bool(uint64_t)> check_method )
{
	if (required && value == "")
		throw invalid_argument(error_msg);
	uint64_t ret = default_;
	try {
		if (value != "")
			ret = std::stoull(value);
	} catch (exception& e) {
		throw invalid_argument(error_msg);
	}
	if (check_method != nullptr && !check_method(ret))
		throw invalid_argument(error_msg);
	return ret;
}

double parseDouble(const string &value, const bool required, const double default_,
               const char* error_msg,
			   function<bool(double)> check_method )
{
	if (required && value == "")
		throw invalid_argument(error_msg);
	double ret = default_;
	try {
		if (value != "")
			ret = std::stod(value);
	} catch (exception& e) {
		throw invalid_argument(error_msg);
	}
	if (check_method != nullptr && !check_method(ret))
		throw invalid_argument(error_msg);
	return ret;
}


////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "SystemStat::CPUCounters::"
SystemStat::CPUCounters::CPUCounters(const string& src) {
	auto aux = split_str(src, " ");
	for (int i = 0; i<aux.size(); i++) {
		auto value = parseUint64(aux[i], true, 0);
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
	string out = command_output("uptime");

	std::cmatch cm;
	regex_search(out.c_str(), cm, regex("load average:\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+),\\s+([0-9]+[.,][0-9]+)"));
	if( cm.size() >= 4 ){
		string aux;
		str_replace(aux, cm.str(1), ',', '.');
		load1 = parseDouble(aux, true, 0);
		str_replace(aux, cm.str(2), ',', '.');
		load5 = parseDouble(aux, true, 0);
		str_replace(aux, cm.str(3), ',', '.');
		load15 = parseDouble(aux, true, 0);
	} else {
		throw runtime_error("unable to parse the output of uptime");
	}
}

void SystemStat::getCPU() {
	string out = command_output("cat /proc/stat");
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
