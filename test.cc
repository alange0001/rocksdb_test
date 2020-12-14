// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <regex>
#include <filesystem>

#include <csignal>
#include <errno.h>

#include <spdlog/spdlog.h>
#include <alutils/process.h>
/*
#include <fmt/format.h>
#include <gflags/gflags.h>
*/

#include "util.h"

using namespace std;
using std::chrono::system_clock;
using fmt::format;

int64_t ParetoCDFInversion(double u, double theta, double k, double sigma) {
	double ret;
	if (k == 0.0) {
		ret = theta - sigma * std::log(u);
	} else {
		ret = theta + sigma * (std::pow(u, -1 * k) - 1) / k;
	}
	return static_cast<int64_t>(ceil(ret));
}

int64_t PowerCdfInversion(double u, double a, double b, double c) {
  double ret;
  ret = std::pow(((u - c) / a), (1 / b));
  return static_cast<int64_t>(ceil(ret));
}

int F_i1 = 0;
int F_i2 = 3;
string F_s("test");

struct S {
	int& i1;
	int& i2;
	string& s;
	S() : i1(F_i1), i2(F_i2), s(F_s) {}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

void signalHandler(int signal) {
	spdlog::warn("received signal {}", signal);
	std::signal(signal, SIG_DFL);
	std::raise(signal);
}

int main(int argc, char** argv) {
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGSEGV, signalHandler);
	std::signal(SIGINT,  signalHandler);
	std::signal(SIGILL,  signalHandler);
	std::signal(SIGABRT, signalHandler);
	std::signal(SIGFPE,  signalHandler);
	spdlog::set_level(spdlog::level::debug);
	DEBUG_MSG("Initiating...");

	try {
#		define CODE3
#		if defined CODE1

		S test;
		fmt::print(stdout, "{}\n", test.s);
		F_s = "test2";
		fmt::print(stdout, "{}\n", test.s);
		test.s = "test3";
		fmt::print(stdout, "{}\n", F_s);

#		elif defined CODE2

		//std::this_thread::sleep_for(seconds(50));
		auto ret_aux = std::system(nullptr);
		spdlog::info("command NULL returned: {}", ret_aux);
		
		string cmd("bash -c 'echo $PATH'");
		if (argc > 1)
			cmd = argv[1];
		spdlog::info("executing command: {}", cmd);
		auto ret = alutils::command_output(cmd.c_str());
		spdlog::info("command output: {}", ret);

#		elif defined CODE3

		TmpFileCopy t("/etc/hostname");
		spdlog::info("temporary copy of file {}: {}", t.original_name(), t.name());

#		else

		auto p = alutils::get_children(0);
		for (auto i: p) {
			spdlog::info("child pid: {}", i);
		}


#		endif
	} catch (const std::exception& e) {
		spdlog::error("Exception received: {}", e.what());
		return 1;
	}

	spdlog::info("return 0");
	return 0;
}
