
#include <deque>
#include <map>
#include <algorithm>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "util.h"
#include "args.h"


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

struct Test {
	int i = 0;

	Test() {}
	~Test() {
		spdlog::info("Test destructor");
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

int main(int argc, char** argv) {
	spdlog::set_level(spdlog::level::debug);

	std::unique_ptr<std::unique_ptr<Test>[]> il;

	il.reset(new std::unique_ptr<Test>[5]);
	for (int i=0; i<5; i++) {
		il[i].reset(new Test());
		il[i]->i = i;
		spdlog::info("{}", il[i]->i);
	}
	il.reset(nullptr);

	spdlog::info("return 0");
	return 0;
}
