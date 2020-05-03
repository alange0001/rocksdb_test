
#pragma once
#include <memory>


std::shared_ptr<char[]> formatString(const char* format, ...);

struct Defer {
	void (*method)();
	Defer(void (*method_)()) : method(method_) {}
	~Defer() noexcept(false) {method();}
};

struct Popen2 {
	FILE* f;
	Popen2(const char* cmd);
	~Popen2() noexcept(false);
	char* gets(char* buffer, int size);
};
