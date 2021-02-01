// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <string>

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <alutils/process.h>

#include "util.h"

#include "nlohmann/json.hpp"

using std::string;
using std::runtime_error;
using fmt::format;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ExperimentTask::"

const char* stat_format = "Task {}, STATS: {}";

class ExperimentTask {
	protected: //------------------------------------------------------------------
	string name = "";
	Clock* clock = nullptr;
	nlohmann::ordered_json data;
	std::unique_ptr<alutils::ProcessController> process;
	uint64_t warm_period_s;

	ExperimentTask() {}

	public: //---------------------------------------------------------------------
	ExperimentTask(const string name_, Clock* clock_, uint64_t warm_period_s_=0) : name(name_), clock(clock_), warm_period_s(warm_period_s_) {
		DEBUG_MSG("constructor of task {}", name);
		if (clock == nullptr)
			throw runtime_error("invalid clock");
		data["time"] = "";
	}
	virtual ~ExperimentTask() {
		DEBUG_MSG("destructor of task {}", name);
		process.reset(nullptr);
		DEBUG_MSG("destructor finished");
	}
	bool isActive() {
		return (process.get() != nullptr && process->isActive(true));
	}
	void stop() {
		process.reset(nullptr);
	}

	void print() {
		print(data);
	}

	void print(nlohmann::ordered_json& j) {
		if (j.size() == 0)
			spdlog::warn("no data in task {}", name);
		auto clock_s = clock->s();
		if (clock_s > warm_period_s) {
			j["time"] = format("{}", clock_s - warm_period_s);
			spdlog::info(stat_format, name, j.dump());
		}
		j.clear();
		j["time"] = "";
	}

	nlohmann::ordered_json get_data_and_clear() {
		nlohmann::ordered_json j = data;
		data.clear();
		data["time"] = "";
		return j;
	}

	void default_stderr_handler (const char* buffer) {
		spdlog::warn("Task {}, stderr: {}", name, buffer);
	}
};
