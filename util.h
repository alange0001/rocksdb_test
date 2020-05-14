
#pragma once
#include <memory>
#include <algorithm>
#include <functional>
#include <vector>
#include <map>
#include <exception>

#include <atomic>
#include <thread>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ ""
#define DEBUG_MSG(format, ...) spdlog::debug("[{}] " __CLASS__ "{}(): " format, __LINE__, __func__ , ##__VA_ARGS__)
#define DEBUG_OUT(condition, format, ...) \
	if (condition) \
		spdlog::debug("[{}] " __CLASS__ "{}(): " format, __LINE__, __func__ , ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////////

inline std::string& inplace_trim(std::string& src) {
	const char* to_trim = " \t\n\r\f\v";

	src.erase(src.find_last_not_of(to_trim) +1);
	src.erase(0, src.find_first_not_of(to_trim));

	return src;
}

int split_columns(std::vector<std::string>& ret, const char* str, const char* prefix=nullptr);

inline std::string str_replace(const std::string& src, const char find, const char replace) {
	std::string dest = src;
	std::replace(dest.begin(), dest.end(), find, replace);
	return dest;
}

inline std::string& str_replace(std::string& dest, const std::string& src, const char find, const char replace) {
	dest = src;
	std::replace(dest.begin(), dest.end(), find, replace);
	return dest;
}

bool parseBool(const std::string &value, const bool required=true, const bool default_=true,
               const char* error_msg="invalid value (boolean)",
			   std::function<bool(bool)> check_method=nullptr );
uint64_t parseUint(const std::string &value, const bool required=true, const uint64_t default_=0,
               const char* error_msg="invalid value (uint64)",
			   std::function<bool(uint64_t)> check_method=nullptr );
double parseDouble(const std::string &value, const bool required=true, const double default_=0.0,
               const char* error_msg="invalid value (double)",
			   std::function<bool(double)> check_method=nullptr );

bool monitor_fgets (char* buffer, int buffer_size, std::FILE* file, bool* stop, uint64_t interval=300);

std::string command_output(const char* cmd, bool debug_out=false);

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Defer::"

struct Defer {
	std::function<void()> method;
	Defer(std::function<void()> method_) : method(method_) {}
	~Defer() noexcept(false) {method();}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ProcessController::"

class ProcessController {
	std::string name;
	bool        debug_out;

	bool must_stop      = false;
	bool thread_active  = false;
	bool program_active = false;

	pid_t        pid     = 0;
	std::FILE*   f_read  = nullptr;
	std::FILE*   f_write = nullptr;

	std::thread        thread;
	std::exception_ptr thread_exception;

	std::function<void(const char*)> handler;

	void threadMain() noexcept;
	bool checkStatus() noexcept;

	public: //---------------------------------------------------------------------
	ProcessController(const char* name_, const char* cmd, std::function<void(const char*)> handler_,
			bool debug_out_=false);
	~ProcessController();

	bool puts(const std::string value) noexcept;

	bool isActive(bool throwexcept=false);
	int  exit_code      = 0;
	int  signal         = 0;
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Exception::"

class Exception : public std::exception {
	public:
	std::string msg;
	Exception(const char* msg_);
	Exception(const std::string& msg_);
	Exception(const Exception& src);
	Exception& operator=(const Exception& src);
	virtual const char* what() const noexcept;
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ExperimentTask::"

class ExperimentTask {
	protected: //------------------------------------------------------------------
	std::string name = "";
	std::map<std::string, std::string> data;

	std::unique_ptr<ProcessController> process;

	ExperimentTask() {}

	public: //---------------------------------------------------------------------
	ExperimentTask(const std::string name_) : name(name_) {
		DEBUG_MSG("constructor of task {}", name);
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
};

#undef __CLASS__
#define __CLASS__ ""
