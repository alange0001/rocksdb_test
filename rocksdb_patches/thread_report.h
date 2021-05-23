// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#pragma once

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

class ThreadReport {
	public:
	ThreadReport(){}
	virtual ~ThreadReport(){}
};

#define DECLARE_THREADREPORT_POINTER             \
  private:                                       \
  std::unique_ptr<ThreadReport> thread_report
