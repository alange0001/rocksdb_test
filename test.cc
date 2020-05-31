
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <regex>

#include <csignal>
#include <errno.h>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include <gflags/gflags.h>

#include "util.h"
#include "process.h"

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
#		define CODE1
#		ifdef CODE1

		S test;
		fmt::print(stdout, "{}\n", test.s);
		F_s = "test2";
		fmt::print(stdout, "{}\n", test.s);
		test.s = "test3";
		fmt::print(stdout, "{}\n", F_s);

#		else

		auto flags = std::regex_constants::match_any;
		std::cmatch cm;
		const char* prefix = "cpu1";
		regex_search(text, cm, regex(format("{} +(.*)", prefix)), flags);
		if (cm.size() >= 2) {
			auto aux = split_str(cm.str(1), " ");
			for (auto i: aux) {
				spdlog::info("{}: {}", prefix, i);
			}
		}

#		endif
	} catch (const std::exception& e) {
		spdlog::error("Exception received: {}", e.what());
		return 1;
	}

	spdlog::info("return 0");
	return 0;
}
