// Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <map>
#include <deque>
#include <algorithm>
#include <chrono>
#include <type_traits>

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <alutils/print.h>
#include <alutils/string.h>

using std::string;
using std::vector;
using std::function;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::exception;
using std::invalid_argument;
using std::runtime_error;
using fmt::format;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

#define DEBUG_MSG(format, ...) spdlog::debug("[{}] " __CLASS__ "{}(): " format, __LINE__, __func__ , ##__VA_ARGS__)
#define DEBUG_OUT(format, ...) \
	if (loglevel.level <= Log::LOG_DEBUG_OUT) \
		spdlog::debug("[{}] " __CLASS__ "{}(): " format, __LINE__, __func__ , ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////////

struct LogLevel {
	typedef enum {
		LOG_DEBUG_OUT,
		LOG_DEBUG,
		LOG_INFO,
		N_levels
	} levels;
	const char* map_names[N_levels+1] = {
			"output",
			"debug",
			"info", NULL};
	const alutils::log_level_t map_alutils[N_levels] = {
			alutils::LOG_DEBUG_OUT,
			alutils::LOG_DEBUG,
			alutils::LOG_INFO};
	const spdlog::level::level_enum map_spdlog[N_levels] = {
			spdlog::level::debug,
			spdlog::level::debug,
			spdlog::level::info};

	levels level;

	LogLevel();
	void set(const string& name);
};

extern LogLevel loglevel;

////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T sum(const vector<T>& src) {
	T ret = 0;
	for (auto i: src) {
		ret += i;
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "Defer::"

struct Defer {
	function<void()> method;
	Defer(function<void()> method_) : method(method_) {}
	~Defer() noexcept(false) {method();}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "OrderedDict::"

class OrderedDict {
public:
	typedef string                  strType;
	typedef string                  keyType;
	typedef string                  valueType;
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
			ret += format("{}{}={}", (ret.length() > 0) ? ", " : "", i.first, i.second);
		return ret;
	}
	strType json() {
		strType ret;
		for (auto i : *this)
			ret += format("{}\"{}\":\"{}\"", (ret.length() > 0) ? ", " : "", i.first, i.second);
		return format("{} {} {}", '{', ret, '}');
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
#define __CLASS__ "Clock::"

struct Clock {
	system_clock::time_point time_init;

	Clock() { reset(); }
	Clock(Clock&) = default;
	Clock& operator= (Clock&) = default;

	void reset() {
		time_init = system_clock::now();
	}
	uint64_t s() {
		auto time_cur = system_clock::now();
		return std::chrono::duration_cast<seconds>(time_cur - time_init).count();
	}
	uint64_t ms() {
		auto time_cur = system_clock::now();
		return std::chrono::duration_cast<milliseconds>(time_cur - time_init).count();
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "VectorParser::"

template <typename T>
class VectorParser : public vector<T> {
protected:
	const char*  name;
	const char*  delimiter;
	function<bool(T)> check;
	uint32_t* num;
public:
	VectorParser()
		: name(""), delimiter(","), check(nullptr), num(nullptr), vector<T>() {}
	VectorParser(const char* name, const char* delimiter, function<bool(T)> check=nullptr, uint32_t* num=nullptr)
		: name(name), delimiter(delimiter), check(check), num(num), vector<T>() {}

	void configure(const char* name_, const char* delimiter_, function<bool(T)> check_=nullptr, uint32_t* num_=nullptr) {
		name = name_;
		delimiter = delimiter_;
		check = check_;
		num = num_;
	}

	VectorParser<T>& operator=(const string& src) {
		DEBUG_MSG("receiving: {}", src);
		const char* error_msg = "invalid value in the list {}: \"{}\"";

		this->clear();
		if (num != nullptr && *num == 0) return *this;

		auto aux = alutils::split_str(src, delimiter);
		for (auto i: aux) {
			if constexpr (std::is_same<T, string>::value) {
				if (check != nullptr && !check(i))
					throw invalid_argument(format(error_msg, name, i));
				this->push_back( i );
			} else if constexpr (std::is_same<T, uint32_t>::value) {
				this->push_back( alutils::parseUint32(i, true, 0, format(error_msg, name, i).c_str(), check) );
			} else if constexpr (std::is_same<T, uint64_t>::value) {
				this->push_back( alutils::parseUint64(i, true, 0, format(error_msg, name, i).c_str(), check) );
			} else if constexpr (std::is_same<T, double>::value) {
				this->push_back( alutils::parseDouble(i, true, 0, format(error_msg, name, i).c_str(), check) );
			} else {
				throw invalid_argument(format("type not implemented for list {}", name));
			}
		}

		if (num != nullptr) {
			while (*num < this->size()) {
				this->pop_back();
			}
			if (*num > 1 && this->size() > 1 && this->size() < *num)
				throw invalid_argument(format("the list {} must have either one element or {}", name, *num));
			while (*num > this->size()) {
				auto aux2 = this->operator [](0);
				this->push_back(aux2);
			}
		}

		return *this;
	}
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "SystemStat::"

struct SystemStat {
public: //----------------------------------------------------------------------
	struct CPUCounters {
		enum Types {
			S_USER = 0, S_NICE, S_SYSTEM, S_IDLE, S_IOWAIT, S_IRQ, S_SOFTIRQ,
			S_STEAL, S_GUEST, S_GUEST_NICE,
			NUM_COUNTERS
		};
		vector<uint64_t> data;
		uint64_t total = 0;
		uint64_t active = 0;
		uint64_t idle = 0;
		CPUCounters(const string& src);
		CPUCounters(const CPUCounters& src) {*this = src;}
		CPUCounters() {}
		CPUCounters& operator=(const CPUCounters& src) {
			data =src.data; total = src.total; active = src.active; idle =src.idle;
			return *this;
		}
		double getActive(const CPUCounters& prev) const {
			return static_cast<double>(100 * (active - prev.active)) / static_cast<double>(total - prev.total);
		}
		double getByType(const CPUCounters& prev, Types type) const {
			return static_cast<double>(100 * (data[type] - prev.data[type])) / static_cast<double>(total - prev.total);
		}
	};

	bool debug_out = false;
	double load1;
	double load5;
	double load15;

	CPUCounters cpu_all;
	vector<CPUCounters> cpu;

	SystemStat();
	~SystemStat();
	string json(const SystemStat& prev, const string& first_attributes="");

private: //---------------------------------------------------------------------
	void getLoad();
	void getCPU();
};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""
