
#include "util.h"
#include <cstdarg>

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

