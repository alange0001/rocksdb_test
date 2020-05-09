
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

//std::shared_ptr<char[]> formatString(const char* format, ...);

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

bool parseBool(std::string &value, bool* ret);
bool parseUint(std::string &value, uint64_t* ret);
bool parseDouble(std::string &value, double* ret, double min, double max);

////////////////////////////////////////////////////////////////////////////////////

struct Defer {
	std::function<void()> method;
	Defer(std::function<void()> method_) : method(method_) {}
	~Defer() noexcept(false) {method();}
};

////////////////////////////////////////////////////////////////////////////////////

struct Subprocess {
	bool noexceptions_ = false;
	std::string name;
	FILE* f;
	Subprocess(const char* name_, const char* cmd);
	~Subprocess() noexcept(false);
	void noexceptions(bool v=true);
	void close();
	char* gets(char* buffer, int size);
	uint32_t getAll(std::string& ret);
};

////////////////////////////////////////////////////////////////////////////////////

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
			Subprocess subprocess(name, cmd.c_str());

			DEBUG_MSG("collecting output stats in the task {}", name);
			std::string lines_to_parse;
			uint32_t line_count = 0;
			while (!stop_ && subprocess.gets(buffer, buffer_size -1)){
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
				subprocess.close();
			} else if (must_not_finish) {
				subprocess.close();
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
