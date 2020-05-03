
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

Popen2::Popen2(const char* cmd) {
	f = popen(cmd, "r");
	if (f == nullptr)
		throw std::runtime_error(fmt::format("error executing program ({})", cmd));
}

Popen2::~Popen2() noexcept(false) {
	auto exit_code = pclose(f);
	if (exit_code != 0)
		throw std::runtime_error(fmt::format("db_bench exit error {}", exit_code));
}

char* Popen2::gets(char* buffer, int size) {
	return fgets(buffer, size, f);
}
