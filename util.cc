
#include "util.h"

#include <cstdarg>
#include <stdexcept>
#include <regex>

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

int split_columns(std::vector<std::string>& ret, const char* str, const char* prefix) {
	std::cmatch cm;
	auto flags = std::regex_constants::match_any;
	std::string str_aux;

	ret.clear();

	if (prefix != nullptr) {
		std::regex_search(str, cm, std::regex(fmt::format("{}\\s+(.+)", prefix).c_str()), flags);
		//print_cm(cm);
		if (cm.size() < 2)
			return 0;

		str_aux = cm[1].str();
		str = str_aux.c_str();
	}

	for (const char* i = str;;) {
		std::regex_search(i, cm, std::regex("([^\\s]+)\\s*(.*)"), flags);
		//print_cm(cm);
		if (cm.size() >= 3) {
			ret.push_back(cm[1].str());
			i = cm[2].first;
		} else {
			break;
		}
	}

	return ret.size();
}

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "Subprocess::"

Subprocess::Subprocess(const char* name_, const char* cmd) : name(name_) {
	DEBUG_MSG("popen process {}", name);
	f = popen(cmd, "r");
	if (f == NULL)
		throw Exception(fmt::format("error executing subprocess {}, command: {}", name, cmd));
}

Subprocess::~Subprocess() noexcept(false) {
	if (f) {
		DEBUG_MSG("pclose process {}", name);
		auto exit_code = pclose(f);
		if (exit_code != 0) {
			if (noexceptions_ || std::current_exception())
				spdlog::error(fmt::format("subprocess {} exit error {}", name, exit_code));
			else
				throw Exception(fmt::format("subprocess {} exit error {}", name, exit_code));
		}
	}
}

void Subprocess::close() {
	if (f) {
		DEBUG_MSG("pclose process {}", name);
		pclose(f);
		f = NULL;
	}
}

void Subprocess::noexceptions(bool v) {
	noexceptions_ = v;
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

////////////////////////////////////////////////////////////////////////////////////

#undef __CLASS__
#define __CLASS__ "Exception::"

Exception::Exception(const char* msg_) : msg(msg_) {
	DEBUG_MSG("Exception created: {}", msg_);
}
Exception::Exception(const std::string& msg_) : msg(msg_) {
	DEBUG_MSG("Exception created: {}", msg_);
}
Exception::Exception(const Exception& src) {
	*this = src;
}
Exception& Exception::operator=(const Exception& src) {
	msg = src.msg;
	return *this;
}
const char* Exception::what() const noexcept {
	return msg.c_str();
}

