
#include "args.h"

#include <string>
#include <stdexcept>
#include <functional>

#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "util.h"

using std::string;
using std::runtime_error;
using std::invalid_argument;
using std::function;
using fmt::format;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

////////////////////////////////////////////////////////////////////////////////////
#define DEFINE_uint32_t uint32_t
#define DEFINE_uint64_t uint64_t
#define DEFINE_string_t string&
#define DEFINE_bool_t bool
#define declareFlag(ARG_name, ARG_type, ARG_flag_type, ARG_flag_default, ARG_help, ARG_condition, ARG_set_event, ...) \
	ARG_flag_type (ARG_name, ARG_flag_default, ARG_help);                          \
	static bool validate_##ARG_name(const char* flagname, const ARG_flag_type##_t value) { \
		if (!(ARG_condition)) {                                                    \
			throw std::invalid_argument(fmt::format(                               \
				"Invalid value for the parameter {}: \"{}\"."                      \
				"Condition: " #ARG_condition,                                      \
				flagname, value));                                                 \
		}                                                                          \
		ARG_set_event;                                                             \
		return true;                                                               \
	}                                                                              \
	//DEFINE_validator(ARG_name, &validate_##ARG_name)
////////////////////////////////////////////////////////////////////////////////////

ALL_ARGS_F( declareFlag );


////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

Args* Args::this_ = nullptr;

Args::Args(int argc, char** argv){
	if (Args::this_ != nullptr)
		throw runtime_error("Args already initiated");
	Args::this_ = this;

	gflags::SetUsageMessage(string("\nUSAGE:\n\t") + string(argv[0]) +
				" [OPTIONS]...");
	spdlog::set_level(spdlog::level::info);
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	string params_str;
#	define printParam(ARG_name, ...) params_str += format("{}--" #ARG_name "=\"{}\"", (params_str.length()>0)?" ":"", FLAGS_##ARG_name)
	ALL_ARGS_F( printParam );
#	undef printParam
	spdlog::info("parameters: {}", params_str);


#	define assignValue(ARG_name, ...) \
		ARG_name = FLAGS_##ARG_name
	ALL_ARGS_Direct_F( assignValue );
#	undef assignValue

#	define assignList(ARG_name, ARG_type, ARG_flag_type, ARG_flag_default, ARG_help, ARG_condition, ARG_set_event, ARG_item_type, ARG_item_condition, ARG_items, ...) \
		ARG_name = parseList_##ARG_item_type(FLAGS_##ARG_name, #ARG_name, ARG_items, [](const ARG_item_type value)->bool{return (ARG_item_condition);})
	ALL_ARGS_List_F( assignList );
#	undef assignList

	checkUniqueStr("db_path", db_path);
	checkUniqueStr("at_file", at_file);

#	define print_arg(ARG_name, ...) \
		spdlog::info("Args." #ARG_name ": {}", ARG_name)
#	define print_arg_list(ARG_name, ...) \
		for (int i=0; i<ARG_name.size(); i++) \
			spdlog::info("Args." #ARG_name "[{}]: {}", i, ARG_name[i])
	ALL_ARGS_Direct_F( print_arg );
	ALL_ARGS_List_F( print_arg_list );
#	undef print_arg
#	undef print_arg_list
}

Args::~Args() {
	Args::this_ = nullptr;
}

void Args::setLogLevel(const string& value) {
	if      (FLAGS_log_level == "debug"   ) spdlog::set_level(spdlog::level::debug);
	else if (FLAGS_log_level == "info"    ) spdlog::set_level(spdlog::level::info);
	else if (FLAGS_log_level == "warn"    ) spdlog::set_level(spdlog::level::warn);
	else if (FLAGS_log_level == "error"   ) spdlog::set_level(spdlog::level::err);
	else if (FLAGS_log_level == "critical") spdlog::set_level(spdlog::level::critical);
	else if (FLAGS_log_level == "off"     ) spdlog::set_level(spdlog::level::off);
	else throw invalid_argument(format("invalid log_level: {}", FLAGS_log_level));
}

vector<string> Args::parseList_string(
		const string& str, const char* name, const uint32_t num,
		function<bool(const string)> check)
{
	if (num == 0) return vector<string>();
	vector<string> ret = split_str(str, param_delimiter);
	if (check != nullptr) {
		for (auto i: ret)
			if (!check(i))
				throw invalid_argument(format("check error in the list {}: {}", name, i));
	}
	if (num < ret.size())
		throw invalid_argument(format("the list {} is greater than {}", name, num));
	if (num > 1 && ret.size() > 1 && ret.size() < num)
		throw invalid_argument(format("the list {} must have either one element or {}", name, num));
	while (num > ret.size()) {
		ret.push_back(ret[0]);
	}
	return ret;
}

vector<uint32_t> Args::parseList_uint32_t(
		const string& str, const char* name, const uint32_t num,
		function<bool(const uint32_t v)> check)
{
	if (num == 0) return vector<uint32_t>();
	auto aux = split_str(str, param_delimiter);
	vector<uint32_t> ret;
	for (auto i: aux) {
		ret.push_back( parseUint32(i, true, 0, format("invalid value in the list {}: {}", name, i).c_str(), check) );
	}
	if (num < ret.size())
		throw invalid_argument(format("the list {} is greater than {}", name, num));
	if (num > 1 && ret.size() > 1 && ret.size() < num)
		throw invalid_argument(format("the list {} must have either one element or {}", name, num));
	while (num > ret.size()) {
		ret.push_back(ret[0]);
	}
	return ret;
}

vector<uint64_t> Args::parseList_uint64_t(
		const string& str, const char* name, const uint32_t num,
		function<bool(const uint64_t v)> check)
{
	if (num == 0) return vector<uint64_t>();
	auto aux = split_str(str, param_delimiter);
	vector<uint64_t> ret;
	for (auto i: aux) {
		ret.push_back( parseUint64(i, true, 0, format("invalid value in the list {}: {}", name, i).c_str(), check) );
	}
	if (num < ret.size())
		throw invalid_argument(format("the list {} is greater than {}", name, num));
	if (num > 1 && ret.size() > 1 && ret.size() < num)
		throw invalid_argument(format("the list {} must have either one element or {}", name, num));
	while (num > ret.size()) {
		ret.push_back(ret[0]);
	}
	return ret;
}


void Args::checkUniqueStr(const char* name, const vector<string>& src) {
	for (int i=0; i<src.size(); i++) {
		for (int j=i+1; j<src.size(); j++) {
			if (src[i] == src[j]) throw invalid_argument(format("duplicated entries in {}: {}", name, src[i]));
		}
	}
}
