// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#include "args.h"

#include <string>
#include <stdexcept>
#include <functional>

#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <alutils/string.h>
#include <alutils/print.h>

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
		DEBUG_MSG("flagname={}, value={}", flagname, value);              \
		if (!(ARG_condition)) {                                                    \
			throw std::invalid_argument(fmt::format(                               \
				"Invalid value for the parameter {}: \"{}\". "                     \
				"Condition: " #ARG_condition ".",                                  \
				flagname, value));                                                 \
		}                                                                          \
		ARG_set_event;                                                             \
		return true;                                                               \
	}                                                                              \
	DEFINE_validator(ARG_name, &validate_##ARG_name);
////////////////////////////////////////////////////////////////////////////////////

namespace Log {
	const char* names[] = {"output", "debug", "info", NULL};
	const alutils::log_level_t alutils[] = {alutils::LOG_DEBUG_OUT, alutils::LOG_DEBUG, alutils::LOG_INFO};
	const spdlog::level::level_enum spdlog[] = {spdlog::level::debug, spdlog::level::debug, spdlog::level::info};
}

static void setLogLevel(const string& value) {
	DEBUG_MSG("set log_level to {}", value);

	for (int i=0; Log::names[i] != NULL; i++) {
		if (value == Log::names[i]) {
			Log::level = (Log::levels)i;
			alutils::log_level = Log::alutils[i];
			spdlog::set_level(Log::spdlog[i]);
			return;
		}
	}

	throw invalid_argument(format("invalid log_level: {}", value));
}

ALL_ARGS_F( declareFlag );

////////////////////////////////////////////////////////////////////////////////////
using alutils::vsprintf;
ALUTILS_PRINT_WRAPPER(alu_print_debug,    spdlog::debug("{}", msg));
ALUTILS_PRINT_WRAPPER(alu_print_info,     spdlog::info("{}", msg));
ALUTILS_PRINT_WRAPPER(alu_print_warn,     spdlog::warn("{}", msg));
ALUTILS_PRINT_WRAPPER(alu_print_error,    spdlog::error("{}", msg));
ALUTILS_PRINT_WRAPPER(alu_print_critical, spdlog::critical("{}", msg));

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Args::"

#define initializeList(ARG_name, ARG_type, ARG_flag_type, ARG_flag_default, ARG_help, ARG_condition, ARG_set_event, ARG_item_type, ARG_item_condition, ARG_items) \
		ARG_name(#ARG_name, param_delimiter, [](const ARG_item_type value)->bool{return (ARG_item_condition);}, &ARG_items),

Args::Args(int argc, char** argv) : ALL_ARGS_List_F(initializeList) log_level("info") {

	alutils::log_level       = alutils::LOG_INFO;
	alutils::print_debug_out = alu_print_debug;
	alutils::print_debug     = alu_print_debug;
	alutils::print_info      = alu_print_info;
	alutils::print_notice    = alu_print_info;
	alutils::print_warn      = alu_print_warn;
	alutils::print_error     = alu_print_error;
	alutils::print_critical  = alu_print_critical;

	gflags::SetUsageMessage(string("\nUSAGE:\n\t") + string(argv[0]) +
				" [OPTIONS]...");
	spdlog::set_level(spdlog::level::info);
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	string params_str;
#	define printParam(ARG_name, ...) \
		params_str += format("{}--" #ARG_name "=\"{}\"", (params_str.length()>0)?" ":"", FLAGS_##ARG_name);
	ALL_ARGS_F( printParam );
#	undef printParam
	spdlog::info("parameters: {}", params_str);

#	define assignValue(ARG_name, ...) \
		DEBUG_MSG("assign " #ARG_name " = {}", FLAGS_##ARG_name);\
		ARG_name = FLAGS_##ARG_name;
	ALL_ARGS_F( assignValue );
#	undef assignValue

	checkUniqueStr("db_path", db_path);
	checkUniqueStr("ydb_path", ydb_path);
	checkUniqueStr("at_file", at_file);

#	define print_arg(ARG_name, ...) \
		spdlog::info("Args." #ARG_name ": {}", ARG_name);
#	define print_arg_list(ARG_name, ...) \
		for (int i=0; i<ARG_name.size(); i++) \
			spdlog::info("Args." #ARG_name "[{}]: {}", i, ARG_name[i]);
	ALL_ARGS_Direct_F( print_arg );
	ALL_ARGS_List_F( print_arg_list );
#	undef print_arg
#	undef print_arg_list
}

void Args::checkUniqueStr(const char* name, const vector<string>& src) {
	for (int i=0; i<src.size(); i++) {
		for (int j=i+1; j<src.size(); j++) {
			if (src[i] == src[j]) throw invalid_argument(format("duplicated entries in {}: {}", name, src[i]));
		}
	}
}
