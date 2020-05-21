
#pragma once

#include <string>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "util.h"
#include "process.h"

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ExperimentTask::"

class ExperimentTask {
	protected: //------------------------------------------------------------------
	std::string name = "";
	Clock* clock = nullptr;
	OrderedDict data;
	std::unique_ptr<ProcessController> process;

	ExperimentTask() {}

	public: //---------------------------------------------------------------------
	ExperimentTask(const std::string name_, Clock* clock_) : name(name_), clock(clock_) {
		DEBUG_MSG("constructor of task {}", name);
		if (clock == nullptr)
			throw std::runtime_error("invalid clock");
	}
	virtual ~ExperimentTask() {
		DEBUG_MSG("destructor of task {}", name);
		process.reset(nullptr);
	}
	bool isActive() {
		return (process.get() != nullptr && process->isActive(true));
	}
	void stop() {
		process.reset(nullptr);
	}

	std::string str() {
		std::string s;
		for (auto i : data)
			s += fmt::format("{}{}={}", (s.length() > 0) ? ", " : "", i.first, i.second);
		return s;
	}

	void print() {
		if (data.size() == 0)
			spdlog::warn("no data in task {}", name);
		data.push_front("time", fmt::format("{}", clock->seconds()));
		spdlog::info("Task {}, STATS: {}", name, data.json());
	}

	void default_stderr_handler (const char* buffer) {
		spdlog::warn("Task {}, stderr: {}", name, buffer);
	}
};
