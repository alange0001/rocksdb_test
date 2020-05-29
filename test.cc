
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

		const char* filename = "/media/auto/work/tmp/1";
		const uint32_t filesize = 100;

		const size_t buffer_align = 512;
		const size_t buffer_size = 1024 * 1024;
		alignas(buffer_align) char buffer[buffer_size];

		spdlog::info("creating file {}", filename);
		auto fd = open(filename, O_CREAT|O_WRONLY|O_DIRECT, 0640);
		DEBUG_MSG("fd={}", fd);
		if (fd < 0)
			throw std::runtime_error("can't create file");
		try {
			size_t write_ret;
			for (uint64_t i=0; i<filesize; i++) {
				if ((write_ret = write(fd, buffer, buffer_size)) == -1) {
					throw std::runtime_error(fmt::format("write error: {}", strerror(errno)));
				}
			}
			DEBUG_MSG("file created");
		} catch (std::exception& e) {
			close(fd);
			std::remove(filename);
			throw std::runtime_error(e.what());
		}
		close(fd);

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
