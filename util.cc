
#include "util.h"

#include <cstdarg>
#include <stdexcept>
#include <regex>

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

int split_columns(std::vector<std::string>& ret, const char* str, const char* prefix) {
	std::cmatch cm;
	auto flags = std::regex_constants::match_any;
	std::string str_aux;

	ret.clear();

	if (prefix != nullptr) {
		std::regex_search(str, cm, std::regex(fmt::format("{}\\s+(.+)", prefix).c_str()), flags);
		if (cm.size() < 2)
			return 0;

		str_aux = cm[1].str();
		str = str_aux.c_str();
	}

	for (const char* i = str;;) {
		std::regex_search(i, cm, std::regex("([^\\s]+)\\s*(.*)"), flags);
		if (cm.size() >= 3) {
			ret.push_back(cm[1].str());
			i = cm[2].first;
		} else {
			break;
		}
	}

	return ret.size();
}

bool monitor_fgets (char* buffer, int buffer_size, std::FILE* file, bool* stop, uint64_t interval) {
	struct timeval timeout {0,0};

	auto fd = fileno(file);
	DEBUG_MSG("fd={}", fd);
	fd_set readfds;
	FD_ZERO(&readfds);

	while (!*stop) {
		FD_SET(fd, &readfds);
		auto r = select(fd +1, &readfds, NULL, NULL, &timeout);
		if (r > 0) {
			if (std::fgets(buffer, buffer_size, file) == NULL)
				return false;
			return true;
		}

		if (r < 0)
			throw std::runtime_error("select call error");
		if (std::feof(file))
			return false;
		if (std::ferror(file))
			throw std::runtime_error("file error");

		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}

	return false;
}

bool parseBool(const std::string &value, const bool required, const bool default_,
               const char* error_msg,
			   std::function<bool(bool)> check_method )
{
	const char* true_str[] = {"y","yes","t","true","1", ""};
	const char* false_str[] = {"n","no","f","false","0", ""};
	bool set = (!required && value == "");
	bool ret = default_;

	if (!set) {
		for (const char** i = true_str; **i != '\0'; i++) {
			if (value == *i) {
				ret = true; set = true;
			}
		}
	}
	if (!set) {
		for (const char** i = false_str; **i != '\0'; i++) {
			if (value == *i) {
				ret = false; set = true;
			}
		}
	}

	if (!set)
		throw std::invalid_argument(error_msg);
	if (check_method != nullptr && !check_method(ret))
		throw std::invalid_argument(error_msg);

	return ret;
}

uint64_t parseUint(const std::string &value, const bool required, const uint64_t default_,
               const char* error_msg,
			   std::function<bool(uint64_t)> check_method )
{
	if (required && value == "")
		throw std::invalid_argument(error_msg);
	uint64_t ret = default_;
	try {
		if (value != "")
			ret = std::stoull(value);
	} catch (std::exception& e) {
		throw std::invalid_argument(error_msg);
	}
	if (check_method != nullptr && !check_method(ret))
		throw std::invalid_argument(error_msg);
	return ret;
}

double parseDouble(const std::string &value, const bool required, const double default_,
               const char* error_msg,
			   std::function<bool(double)> check_method )
{
	if (required && value == "")
		throw std::invalid_argument(error_msg);
	double ret = default_;
	try {
		if (value != "")
			ret = std::stod(value);
	} catch (std::exception& e) {
		throw std::invalid_argument(error_msg);
	}
	if (check_method != nullptr && !check_method(ret))
		throw std::invalid_argument(error_msg);
	return ret;
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

