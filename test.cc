
#include <chrono>

//#include <stdio.h>
#include <csignal>
#include <sys/wait.h>

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

void line_handler(const char* line) {
	DEBUG_MSG("handler line: {}", str_replace(line, '\n', ' '));
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

int main() {
	spdlog::set_level(spdlog::level::debug);

	//fmt::print(stdout, "{}", command_output("build/access_time3 --log_level=debug --filename=/mnt/work/tmp/0 --create_file=false --block_size=4 --wait=true 2>&1", true));

	//ProcessController p("test1", "build/access_time3 --log_level=debug --filename=/mnt/work/tmp/0 --create_file=false --block_size=4 --wait=true 2>&1", line_handler, false);
	ProcessController p("test1", "ls -l", line_handler, false);

	uint count=0;
	while (p.isActive()) {
		if (++count >= 5)
			p.puts("stop\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		if (count >= 5)
			p.puts("stop\n");
	}
	DEBUG_MSG("not active"); //*/

	return 0;
}
