
#include "util.h"

#include <stdexcept>
#include <regex>
#include <limits>

using std::string;
using std::vector;
using std::function;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using fmt::format;
using std::exception;
using std::invalid_argument;
using std::runtime_error;

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
