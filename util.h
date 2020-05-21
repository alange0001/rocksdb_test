
#pragma once

#include <map>
#include <deque>
#include <algorithm>
#include <chrono>

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

inline std::string& inplace_strip(std::string& src) {
	const char* to_strip = " \t\n\r\f\v";

	src.erase(src.find_last_not_of(to_strip) +1);
	src.erase(0, src.find_first_not_of(to_strip));

	return src;
}

std::string strip(const std::string& src);

int split_columns(std::vector<std::string>& ret, const char* str, const char* prefix=nullptr);

std::vector<std::string> split_str(const std::string& str, const std::string& delimiter);

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
uint32_t parseUint32(const std::string &value, const bool required=true, const uint32_t default_=0,
               const char* error_msg="invalid value (uint64)",
			   std::function<bool(uint32_t)> check_method=nullptr );
uint64_t parseUint64(const std::string &value, const bool required=true, const uint64_t default_=0,
               const char* error_msg="invalid value (uint64)",
			   std::function<bool(uint64_t)> check_method=nullptr );
double parseDouble(const std::string &value, const bool required=true, const double default_=0.0,
               const char* error_msg="invalid value (double)",
			   std::function<bool(double)> check_method=nullptr );

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

#undef __CLASS__
#define __CLASS__ ""
