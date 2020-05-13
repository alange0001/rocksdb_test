
#pragma once
#include <memory>
#include <algorithm>
#include <functional>
#include <vector>
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

bool monitor_fgets (char* buffer, int buffer_size, std::FILE* file, bool* stop, uint64_t interval=300);

bool parseBool(const std::string &value, const bool required=true, const bool default_=true,
               const char* error_msg="invalid value (boolean)",
			   std::function<bool(bool)> check_method=nullptr );
uint64_t parseUint(const std::string &value, const bool required=true, const uint64_t default_=0,
               const char* error_msg="invalid value (uint64)",
			   std::function<bool(uint64_t)> check_method=nullptr );
double parseDouble(const std::string &value, const bool required=true, const double default_=0.0,
               const char* error_msg="invalid value (double)",
			   std::function<bool(double)> check_method=nullptr );

////////////////////////////////////////////////////////////////////////////////////

struct Defer {
	std::function<void()> method;
	Defer(std::function<void()> method_) : method(method_) {}
	~Defer() noexcept(false) {method();}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Subprocess::"

class Subprocess {
	std::string name;
	std::FILE*  f_out;
	bool        nodestructorexcept;

	bool               stop = false;
	std::thread        thread;
	bool               thread_active = false;
	std::exception_ptr thread_exception;
	std::function<void(const char*)> input_handler;

	const uint32_t          buffer_size = 512;
	std::unique_ptr<char[]> buffer;

	public: //---------------------------------------------------------------------

	Subprocess(const char* name_, const char* cmd, bool nodestructorexcept_=true) : name(name_), nodestructorexcept(nodestructorexcept_) {
		DEBUG_MSG("constructor. popen process {}", name);
		f_out = popen(cmd, "r");
		if (f_out == NULL)
			throw std::runtime_error(fmt::format("error executing subprocess {}, command: {}", name, cmd));
		DEBUG_MSG("constructor finished", name);
	}
	~Subprocess() noexcept(false) {
		DEBUG_MSG("destructor");
		stop = true;
		if (thread.joinable())
			thread.join();
		if (f_out) {
			DEBUG_MSG("pclose process {}", name);
			auto exit_code = pclose(f_out);
			if (exit_code != 0) {
				if (nodestructorexcept || std::current_exception())
					spdlog::error(fmt::format("subprocess {} exit error {}", name, exit_code));
				else
					throw std::runtime_error(fmt::format("subprocess {} exit error {}", name, exit_code));
			}
		}
		DEBUG_MSG("destructor finished");
	}

	char* gets(char* buffer_, int size) {
		if (thread_active)
			throw std::runtime_error("input handler active");
		return fgets(buffer_, size, f_out);
	}
	bool getsOrStop(char* buffer_, int size, bool* stop_, uint64_t interval=300) {
		if (thread_active)
			throw std::runtime_error("input handler active");
		return monitor_fgets(buffer_, size, f_out, stop_, interval);
	}
	uint32_t getAll(std::string& ret) {
		if (thread_active)
			throw std::runtime_error("input handler active");

		buffer.reset(new char[buffer_size]); buffer[0] = '\0'; buffer[buffer_size -1] = '\0';
		ret = "";

		while (gets(buffer.get(), buffer_size -1)) {
			ret += buffer.get();
		}
		return ret.length();
	}

	void threadMain() noexcept {
		DEBUG_MSG("initiate");
		thread_active = true;

		buffer.reset(new char[buffer_size]); buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

		try {
			while (f_out != NULL && monitor_fgets(buffer.get(), buffer_size -1, f_out, &stop)) {
				DEBUG_MSG("call handler");
				input_handler(buffer.get());
				DEBUG_MSG("handler exit");
			}
		} catch(std::exception& e) {
			DEBUG_MSG("exception received: {}", e.what());
			thread_exception = std::current_exception();
			nodestructorexcept = true;
		}
		thread_active = false;
		DEBUG_MSG("finish");
	}
	void registerInputHandler(std::function<void(const char*)> handler) {
		DEBUG_MSG("register handler");
		input_handler = handler;
		thread = std::thread( [this]{this->threadMain();} );
	}
	bool isActive() {
		if (thread_exception)
			std::rethrow_exception(thread_exception);
		return thread_active;
	}
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

enum StatsParseLine {
	DONT_USE,
	USE_LINE,
	USE_LINE_END
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ExperimentTask::"

template<class TStats> class ExperimentTask {
	protected: //------------------------------------------------------------------
	const char* name = "";

	std::thread thread;
	bool stop_ = false;
	bool finished_ = false;
	bool must_not_finish = true;

	bool debug_out = false;
	void* stats_params = nullptr;

	std::exception_ptr exception;

	std::atomic_flag stats_lock;
	uint64_t         stats_consumed = 0;
	uint64_t         stats_produced = 0;
	TStats           stats;

	ExperimentTask() {}

	public: //---------------------------------------------------------------------
	ExperimentTask(const char* name_) : name(name_) {
		DEBUG_MSG("constructor of task {}", name);
		stats_lock.clear();
	}
	virtual ~ExperimentTask() {
		DEBUG_MSG("destructor of task {}", name);
		stop_ = true;
		if (thread.joinable())
			thread.join();
	}

	void launchThread() {
		thread = std::thread( [this]{this->threadMain();} );
	}
	void threadMain() noexcept { // thread function to control the iostat output
		DEBUG_MSG("controller thread initiated for the task {}", name);
		try {
			const int buffer_size = 512;
			char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

			std::string cmd = getCmd();
			std::unique_ptr<Subprocess> subprocess;
			subprocess.reset(new Subprocess(name, cmd.c_str(), true));

			DEBUG_MSG("collecting output stats in the task {}", name);
			std::string lines_to_parse;
			uint32_t line_count = 0;
			while (subprocess->getsOrStop(buffer, buffer_size -1, &stop_)){
				auto parse_this_line = parseThisLine(buffer);

				if (parse_this_line == DONT_USE) {
					if (debug_out) {
						for (char* b = buffer; *b != '\0'; b++) if (*b=='\n') *b = ' ';
						DEBUG_OUT(debug_out, "({}) line not used: {}", name, buffer);
					}
					continue;
				}

				if (parse_this_line == USE_LINE || parse_this_line == USE_LINE_END) {
					lines_to_parse += buffer;
					if (debug_out) {
						for (char* b = buffer; *b != '\0'; b++) if (*b=='\n') *b = ' ';
						DEBUG_OUT(debug_out, "({})     line used: {}", name, buffer);
					}
					if (++line_count > 30) // sanity check
						throw Exception(fmt::format("Lines to parse overflow in the task {}. Please check.", name));
				}

				if (parse_this_line == USE_LINE_END) {
					TStats collect_stats(lines_to_parse.c_str(), stats_params);
					line_count = 0;
					lines_to_parse.clear();

					while (stats_lock.test_and_set());
					stats = collect_stats;
					stats_produced++;
					stats_lock.clear();
				}
			}

			if (stop_) {
				subprocess.reset(nullptr);
			} else if (must_not_finish) {
				subprocess.reset(nullptr);
				throw Exception(fmt::format("{} must not finish in the middle of the experiment", name));
			}

		} catch (std::exception& e) {
			DEBUG_MSG("exception catched: {}", e.what());
			exception = std::current_exception();
		}

		finished_ = true;
		DEBUG_MSG("task thread {} finished", name);
	}

	bool consumeStats(TStats& ret) { // call from the main thread
		if (exception) {
			DEBUG_MSG("rethrow exception");
			std::rethrow_exception(exception);
		}

		while (stats_lock.test_and_set());
		if (stats_consumed < stats_produced) {
			ret = stats;
			stats_consumed = stats_produced;
			stats_lock.clear();
			DEBUG_MSG("stats consumed");
			return true;
		}
		stats_lock.clear();

		return false;
	}
	bool finished() { // call from the main thread
		if (exception) {
			DEBUG_MSG("rethrow exception");
			std::rethrow_exception(exception);
		}
		return finished_;
	}
	void stop() {
		stop_ = true;
	}

	protected: //---------------------------------------------------------------------
	virtual std::string getCmd() { //inherit this method
		throw Exception(fmt::format("getCmd not inherited for the task {}", name));
		return std::string();
	}
	virtual StatsParseLine parseThisLine(const char* buffer) { //inherit this method
		if (stop_)
			return DONT_USE;

		throw Exception(fmt::format("parseThisLine not inherited for the task {}", name));
		return USE_LINE_END;
	}
};

#undef __CLASS__
#define __CLASS__ ""
