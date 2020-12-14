// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#include "util.h"

#include <cstdlib>
#include <stdexcept>
#include <regex>
#include <limits>
#include <filesystem>

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
#undef __CLASS__
#define __CLASS__ ""

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

	set(map_names[LOG_INFO]);
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
#define __CLASS__ "TmpFileCopy::"

TmpFileCopy::TmpFileCopy(const string& original_file_): original_file(original_file_) {
	DEBUG_MSG("constructor");
	{
		char fname[100];
		auto templ = std::filesystem::temp_directory_path() / "rocksdb_test.XXXXXX";
		strcpy(fname, templ.c_str());
		auto s = mktemp(fname);
		if (strlen(s) == 0)
			throw std::runtime_error("failed to create the temporary file");
		DEBUG_MSG("tmp_file: {}", fname);
		tmp_file = fname;
	}
	std::filesystem::copy_file(original_file, tmp_file);
	DEBUG_MSG("file {} copied to {}", original_file, tmp_file);
}

TmpFileCopy::~TmpFileCopy() {
	DEBUG_MSG("destructor");
	std::filesystem::remove(tmp_file);
	DEBUG_MSG("tmp_file removed: {}", tmp_file);
}

const char* TmpFileCopy::original_name() {
	return original_file.c_str();
}

const char* TmpFileCopy::name() {
	return tmp_file.c_str();
}
