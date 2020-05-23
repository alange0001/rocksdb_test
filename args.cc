
#include "args.h"

#include <string>
#include <stdexcept>

#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "util.h"

using std::runtime_error;
using std::invalid_argument;
using fmt::format;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

////////////////////////////////////////////////////////////////////////////////////
#define declareFlag(name)                                        \
	ARG__##name##__ft (name, ARG__##name##__d, ARG__##name##__fm)

#define defaultValidatorMsg "Invalid {}: {}"

#define createValidator(name, condition, message)                                    \
	static bool validate_##name(const char* flagname, const ARG__##name##__t value) { \
	   if (condition)                                                                \
	     return true;                                                                \
	   throw invalid_argument(format(message, flagname, value));           \
	}                                                                                \
	DEFINE_validator(name, &validate_##name)
////////////////////////////////////////////////////////////////////////////////////

ALL_ARGS_F(declareFlag);

createValidator(duration, (value >= 10), defaultValidatorMsg ". Must be >=10");
createValidator(stats_interval, (value > 0), defaultValidatorMsg ". Must be > 0.");
createValidator(num_dbs, (value > 0), defaultValidatorMsg ". Must be > 0.");

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
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	if      (FLAGS_log_level == "debug"   ) spdlog::set_level(spdlog::level::debug);
	else if (FLAGS_log_level == "info"    ) spdlog::set_level(spdlog::level::info);
	else if (FLAGS_log_level == "warn"    ) spdlog::set_level(spdlog::level::warn);
	else if (FLAGS_log_level == "error"   ) spdlog::set_level(spdlog::level::err);
	else if (FLAGS_log_level == "critical") spdlog::set_level(spdlog::level::critical);
	else if (FLAGS_log_level == "off"     ) spdlog::set_level(spdlog::level::off);
	else throw invalid_argument(format("invalid log_level: {}", FLAGS_log_level));

	string params_str;
#	define printParam(name) params_str += format("{}--" #name "=\"{}\"", (params_str.length()>0)?" ":"", FLAGS_##name)
	ALL_ARGS_F(printParam);
#	undef printParam
	spdlog::info("parameters: {}", params_str);


#	define assignList(name) \
		name##_list = ARG__##name##__lf(name(), #name, ARG__##name##__ln(), [](const ARG__##name##__lt value)->bool{return ARG__##name##__lc;})

	ALL_ARGS_List_F( assignList );
	checkUniqueStr("db_path", db_path_list);
	checkUniqueStr("at_file", at_file_list);
#	undef assignList


#	define print_arg(name) \
		spdlog::info("Args." #name ": {}", name())
#	define print_arg_list(name) \
		for (int i=0; i<name##_list.size(); i++) \
			spdlog::info("Args." #name "[{}]: {}", i, name##_list[i])
	ALL_ARGS_NoList_F( print_arg );
	ALL_ARGS_List_F( print_arg_list );
#	undef print_arg
#	undef print_arg_list
}

Args::~Args() {
	Args::this_ = nullptr;
}

#define declareGet(name)            \
	ARG__##name##__t Args::name() {  \
		return FLAGS_##name;        \
	}
ALL_ARGS_F( declareGet );
#undef declareGet

vector<string> Args::parseList_string(
		const string& str, const char* name, const uint32_t num,
		function<bool(const string&)> check)
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
