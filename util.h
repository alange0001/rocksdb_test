
#pragma once
#include <memory>
#include <algorithm>
#include <functional>

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ ""
#define DEBUG_MSG(format, ...) spdlog::debug("[{}] " __CLASS__ "{}(): " format, __LINE__, __func__ , ##__VA_ARGS__)
#define DEBUG_OUT(condition, format, ...) \
	if (condition) \
		spdlog::debug("[{}] " __CLASS__ "{}(): " format, __LINE__, __func__ , ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<char[]> formatString(const char* format, ...);

////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////

struct Defer {
	std::function<void()> method;
	Defer(std::function<void()> method_) : method(method_) {}
	~Defer() noexcept(false) {method();}
};

////////////////////////////////////////////////////////////////////////////////////

struct Subprocess {
	FILE* f;
	Subprocess(const char* cmd);
	~Subprocess() noexcept(false);
	char* gets(char* buffer, int size);
	uint32_t getAll(std::string& ret);
};
