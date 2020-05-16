
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

	OrderedDict m;

	fmt::print(stdout, "size={} ", m.size());
	fmt::print(stdout, "test={}\n", m["test"]);
	m["test"] = "0";
	fmt::print(stdout, "size={} ", m.size());
	fmt::print(stdout, "test={}\n", m["test"]);
	m["test4"] = "4";
	m["test3"] = "3";
	m["test2"] = "2";
	m.push_front("test1", "1");
	m.push_front("test2", "22");
	m.push_front("test3", "33");
	m.push_front("test");

	for (auto i : m) {
		fmt::print(stdout, "{}, {}\n", i.first, i.second);
	}

	////////////////////////////////////////////

	std::deque<std::string> v;
	fmt::print(stdout, "v =");
	for (auto i : v) {
		fmt::print(stdout, " {}", i);
	}
	fmt::print(stdout, "\n");
	v.push_back("test1");
	v.push_back("test2");
	v.push_back("test3");
	v.push_front("test4");
	v.push_back("test2");
	v.push_back("test3");
	std::deque<std::string>::iterator aux;
	while ((aux = std::find(v.begin(), v.end(), "test2")) != v.end())
		v.erase(aux);
	while ((aux = std::find(v.begin(), v.end(), "test22")) != v.end())
		v.erase(aux);
	fmt::print(stdout, "v =");
	for (auto i : v) {
		fmt::print(stdout, " {}", i);
	}

	return 0;
}
