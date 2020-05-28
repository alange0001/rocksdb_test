
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <regex>

#include <csignal>

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

	const char* text =
		"cpu  313892 1287 68028 28424789 70547 0 7296 0 0 0\n"
		"cpu0 22943 70 5033 2373361 5259 0 5110 0 0 0\n"
		"cpu1 26435 62 5258 2369911 5576 0 971 0 0 0\n"
		"cpu2 24732 82 4982 2372649 4422 0 446 0 0 0\n"
		"cpu3 34396 120 5572 2360117 6033 0 182 0 0 0\n"
		"cpu4 28420 52 5815 2366623 5142 0 86 0 0 0\n"
		"cpu5 29346 146 5861 2366267 5160 0 34 0 0 0\n"
		"cpu6 23343 92 4844 2373571 5125 0 18 0 0 0\n"
		"cpu7 22541 82 4886 2373092 5312 0 203 0 0 0\n"
		"cpu8 25123 78 5042 2372701 4140 0 18 0 0 0\n"
		"cpu9 23921 204 9146 2369869 1915 0 11 0 0 0\n"
		"cpu10 27918 160 5639 2370371 3142 0 4 0 0 0\n"
		"cpu11 24771 133 5946 2356252 19315 0 210 0 0 0\n";


	try {
#		define CODE1
#		ifdef CODE1

		SystemStat s1;

		while (true) {
			this_thread::sleep_for(std::chrono::seconds(1));

			SystemStat s2;

			spdlog::info("stats = {}", s2.json(s1));
			s1 = s2;
		}

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
