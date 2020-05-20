
#include <deque>
#include <map>
#include <algorithm>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "util.h"


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

int main() {
	spdlog::set_level(spdlog::level::debug);

	const char* s = "value1; value2; value3";
	//auto l = split_str(s, ";");
	for (auto i: split_str(s, ";")) {
		fmt::print(stdout, "{}\n", i);
	}

	return 0;
}
