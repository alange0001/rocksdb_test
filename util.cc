
#include "util.h"

#include <cstdarg>
#include <stdexcept>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<char[]> formatString(const char* format, ...) {
	va_list va;
	int buffer_size = 256;
	std::shared_ptr<char[]> buffer(new char[buffer_size]);
	va_start(va, format);
	while (std::vsnprintf(buffer.get(), buffer_size, format, va) >= buffer_size -1) {
		buffer_size *= 2;
		if (buffer_size > 1024 * 1024) break;
		buffer.reset(new char[buffer_size]);
		va_start(va, format);
	}
	va_end(va);
	return buffer;
}

////////////////////////////////////////////////////////////////////////////////////

Subprocess::Subprocess(const char* cmd) {
	f = popen(cmd, "r");
	if (f == nullptr)
		throw std::runtime_error(fmt::format("error executing program ({})", cmd));
}

Subprocess::~Subprocess() noexcept(false) {
	auto exit_code = pclose(f);
	if (exit_code != 0)
		throw std::runtime_error(fmt::format("program exit error {}", exit_code));
}

char* Subprocess::gets(char* buffer, int size) {
	return fgets(buffer, size, f);
}

uint32_t Subprocess::getAll(std::string& ret) {
	const int buffer_size = 512;
	char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';
	ret = "";

	while (gets(buffer, buffer_size -1)) {
		ret += buffer;
	}

	return ret.length();
}
