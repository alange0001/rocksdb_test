
#pragma once
#include <memory>
#include <algorithm>
#include <functional>
#include <vector>
#include <deque>
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
	bool program_active = false;

	pid_t        pid     = 0;
	std::FILE*   f_stdin  = nullptr;
	std::FILE*   f_stdout = nullptr;
	std::FILE*   f_stderr = nullptr;

	std::thread        thread_stdout;
	bool               thread_stdout_active  = false;
	std::thread        thread_stderr;
	bool               thread_stderr_active  = false;
	std::exception_ptr thread_exception;

	std::function<void(const char*)> handler_stdout;
	std::function<void(const char*)> handler_stderr;

	void threadStdout() noexcept;
	void threadStderr() noexcept;
	bool checkStatus() noexcept;

	public: //---------------------------------------------------------------------
	ProcessController(const char* name_, const char* cmd,
			std::function<void(const char*)> handler_stdout_=ProcessController::default_stdout_handler,
			std::function<void(const char*)> handler_stderr_=ProcessController::default_stderr_handler,
			bool debug_out_=false);
	~ProcessController();

	bool puts(const std::string value) noexcept;

	bool isActive(bool throwexcept=false);
	int  exit_code      = 0;
	int  signal         = 0;

	static void default_stderr_handler(const char* v) { std::fputs(v, stderr); }
	static void default_stdout_handler(const char* v) { std::fputs(v, stdout); }
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Clock::"

struct Clock {
	std::chrono::system_clock::time_point time_init;
	Clock() { reset(); }
	void reset() {
		time_init = std::chrono::system_clock::now();
	}
	uint32_t seconds() {
		auto time_cur = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::seconds>(time_cur - time_init).count();
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "OrderedDict::"

class OrderedDict {
public:
	typedef std::string                  strType;
	typedef std::string                  keyType;
	typedef std::string                  valueType;
	typedef std::deque<keyType>          orderType;
	typedef std::map<keyType, valueType> dataType;

private:
	orderType order;
	dataType  data;

public:
	OrderedDict() {}
	OrderedDict(const OrderedDict& src) {*this = src;}
	OrderedDict& operator=(const OrderedDict& src) {
		order = src.order;
		data = src.data;
		return *this;
	}
	valueType& operator[](const keyType& key) {
		if (data.find(key) == data.end())
			order.push_back(key);
		return data[key];
	}
	int size() {
		return data.size();
	}
	void clear() {
		data.clear();
		order.clear();
	}
	void push_front(const keyType& key) {
		orderType::iterator aux;
		while ((aux = std::find(order.begin(), order.end(), key)) != order.end())
			order.erase(aux);
		order.push_front(key);
		valueType& s = data[key];
	}
	void push_front(const keyType& key, const valueType& value) {
		orderType::iterator aux;
		while ((aux = std::find(order.begin(), order.end(), key)) != order.end())
			order.erase(aux);
		order.push_front(key);
		data[key] = value;
	}

	strType str() {
		strType ret;
		for (auto i : *this)
			ret += fmt::format("{}{}={}", (ret.length() > 0) ? ", " : "", i.first, i.second);
		return ret;
	}
	strType json() {
		strType ret;
		for (auto i : *this)
			ret += fmt::format("{}\"{}\":\"{}\"", (ret.length() > 0) ? ", " : "", i.first, i.second);
		return fmt::format("{} {} {}", '{', ret, '}');
	}

	struct iterator_item {
		keyType& first;
		valueType& second;
		iterator_item(keyType& first, valueType& second) : first(first), second(second) {}
	};
	class iterator : public std::iterator<std::input_iterator_tag, iterator_item>
	{
		valueType _none_;
		dataType& data;
		orderType& order;
		orderType::iterator order_it;
	public:
		iterator(OrderedDict& src) : data(src.data), order(src.order), order_it(src.order.begin()) {}
		iterator(const iterator& src) : data(src.data), order(src.order), order_it(src.order_it) {}
		iterator& operator++() {++order_it; return *this;}
		iterator operator++(int) {iterator tmp(*this); operator++(); return tmp;}
		bool operator==(const iterator& src) const {return order_it==src.order_it;}
		bool operator!=(const iterator& src) const {return order_it!=src.order_it;}
		void set_end() {order_it = order.end();}
		iterator_item operator*() {
			if (order_it != order.end() && data.find(*order_it) != data.end())
				return iterator_item(*order_it, data[*order_it]);
			else
				return iterator_item(_none_, _none_);
		}
	};
	iterator begin() {return iterator(*this);}
	iterator end() {
		iterator ret(*this);
		ret.set_end();
		return ret;
	}
};

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

#undef __CLASS__
#define __CLASS__ ""
