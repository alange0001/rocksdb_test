
#include <deque>
#include <map>
#include <algorithm>
#include <regex>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "util.h"

using namespace std;
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

int main(int argc, char** argv) {
	spdlog::set_level(spdlog::level::debug);

	const char* buffer = "[2020-05-23 09:15:18.076] [info] STATS: {\"time\":\"60\", \"total_MiB/s\":\"144\", \"read_MiB/s\":\"101\", \"write_MiB/s\":\"43\" \"wait\":\"false\", \"filesize\":\"100000\", \"block_size\":\"4\", \"flush_blocks\":\"1\", \"write_ratio\":\"0.3\", \"random_ratio\":\"0.1\", \"sleep_interval\":\"0\", \"sleep_count\":\"1\"}\n";

	cmatch cm;
	regex_search(buffer, cm, regex("STATS: \\{[^,]+, ([^\\}]+)\\}"));
	if (cm.size() > 1) {
		spdlog::info("{}", cm.str(1));
	}


	spdlog::info("return 0");
	return 0;
}
