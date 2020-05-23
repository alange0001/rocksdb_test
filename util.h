
#pragma once

#include <map>
#include <deque>
#include <algorithm>
#include <chrono>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

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
#define DEBUG_OUT(condition, format, ...) \
	if (condition) \
		spdlog::debug("[{}] " __CLASS__ "{}(): " format, __LINE__, __func__ , ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////////

inline string& inplace_strip(string& src) {
	const char* to_strip = " \t\n\r\f\v";

	src.erase(src.find_last_not_of(to_strip) +1);
	src.erase(0, src.find_first_not_of(to_strip));

	return src;
}

string strip(const string& src);

int split_columns(vector<string>& ret, const char* str, const char* prefix=nullptr);

vector<string> split_str(const string& str, const string& delimiter);

inline string str_replace(const string& src, const char find, const char replace) {
	string dest = src;
	std::replace(dest.begin(), dest.end(), find, replace);
	return dest;
}

inline string& str_replace(string& dest, const string& src, const char find, const char replace) {
	dest = src;
	std::replace(dest.begin(), dest.end(), find, replace);
	return dest;
}

bool parseBool(const string &value, const bool required=true, const bool default_=true,
               const char* error_msg="invalid value (boolean)",
			   function<bool(bool)> check_method=nullptr );
uint32_t parseUint32(const string &value, const bool required=true, const uint32_t default_=0,
               const char* error_msg="invalid value (uint64)",
			   function<bool(uint32_t)> check_method=nullptr );
uint64_t parseUint64(const string &value, const bool required=true, const uint64_t default_=0,
               const char* error_msg="invalid value (uint64)",
			   function<bool(uint64_t)> check_method=nullptr );
double parseDouble(const string &value, const bool required=true, const double default_=0.0,
               const char* error_msg="invalid value (double)",
			   function<bool(double)> check_method=nullptr );

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

#undef __CLASS__
#define __CLASS__ ""
