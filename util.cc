
#include "util.h"

#include <cstdarg>
#include <stdexcept>
#include <regex>

#include <csignal>
#include <sys/wait.h>

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

std::string command_output(const char* cmd, bool debug_out) {
	std::string ret;
	const uint buffer_size = 512;
	char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

	std::FILE* f = popen(cmd, "r");
	if (f == NULL)
		throw std::runtime_error(fmt::format("error executing command \"{}\"", cmd));

	while (std::fgets(buffer, buffer_size -1, f) != NULL) {
		ret += buffer;
		if (debug_out) {
			for (char* i=buffer; *i != '\0'; i++)
				if (*i == '\n') *i = ' ';
			DEBUG_OUT(true, "output line: {}", buffer);
		}
	}

	auto exit_code = pclose(f);
	if (exit_code != 0)
		throw std::runtime_error(fmt::format("command \"{}\" returned error {}", cmd, exit_code ));

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ProcessController::"

ProcessController::ProcessController(const char* name_, const char* cmd, std::function<void(const char*)> handler_, bool debug_out_)
: name(name_), handler(handler_), debug_out(debug_out_)
{
	DEBUG_MSG("constructor. Process {}", name);

	if (handler_ == nullptr)
		throw std::runtime_error(fmt::format("handler not defined for the process {}", name));

	pid_t child_pid;
	int pipe_read[2];
	pipe(pipe_read);
	DEBUG_MSG("pipe_read=({}, {})", pipe_read[0], pipe_read[1]);
	int pipe_write[2];
	pipe(pipe_write);
	DEBUG_MSG("pipe_write=({}, {})", pipe_write[0], pipe_write[1]);

	if ((child_pid = fork()) == -1)
		throw std::runtime_error(fmt::format("fork error on process {}", name));

	if (child_pid == 0) { //// child process ////
		//DEBUG_MSG("child process initiated");
		close(pipe_write[1]);
		dup2(pipe_write[0], STDIN_FILENO);
		close(pipe_read[0]);
		dup2(pipe_read[1], STDOUT_FILENO);

		setpgid(child_pid, child_pid);
		execl("/bin/bash", "/bin/bash", "-c", cmd, (char*)NULL);
		exit(EXIT_FAILURE);
	}

	program_active = true;

	DEBUG_MSG("child pid={}", child_pid);
	pid   = child_pid;
	close(pipe_read[1]);
	if ((f_read = fdopen(pipe_read[0], "r")) == NULL)
		throw std::runtime_error(fmt::format("fdopen (pipe_read) error on subprocess {}", name));
	close(pipe_write[0]);
	if ((f_write  = fdopen(pipe_write[1], "w")) == NULL)
		throw std::runtime_error(fmt::format("fdopen (pipe_write) error on subprocess {}", name));

	thread_active = true;
	thread = std::thread( [this]{this->threadMain();} );
	DEBUG_MSG("constructor finished", name);
}

ProcessController::~ProcessController() {
	DEBUG_MSG("destructor");

	must_stop = true;

	if (checkStatus()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		spdlog::warn("process {} (pid {}) still active. kill it", name, pid);
		//kill(pid, SIGTERM);
		killpg(pid, SIGTERM);
	}

	auto status_f_read = std::fclose(f_read);
	auto status_f_write = std::fclose(f_write);
	DEBUG_MSG("status_f_read={}, status_f_write={}", status_f_read, status_f_write);

	if (thread.joinable())
		thread.join();

	DEBUG_MSG("destructor finished");
}

bool ProcessController::isActive(bool throwexcept) {
	if (thread_exception) {
		if (throwexcept)
			std::rethrow_exception(thread_exception);
		else {
			try { std::rethrow_exception(thread_exception); }
			catch (std::exception& e) {
				spdlog::error("thread exception of program {}: {}", name, e.what());
			}
		}
	}
	bool aux_status = checkStatus();
	if (throwexcept && !aux_status) {
		if (exit_code != 0)
			throw std::runtime_error(fmt::format("program {} exit code {}", name, exit_code));
		if (signal != 0)
			throw std::runtime_error(fmt::format("program {} exit with signal {}", name, signal));
	}
	return thread_active && aux_status;
}

bool ProcessController::puts(const std::string value) noexcept {
	DEBUG_MSG("write: {}", value);
	if (!checkStatus()) {
		spdlog::error("puts failed. Process {} is not active", name);
		return false;
	}
	if (std::fputs(value.c_str(), f_write) == EOF || std::fflush(f_write) == EOF) {
		spdlog::error("fputs/fflush error for process {}", name);
		return false;
	}
	return true;
}

void ProcessController::threadMain() noexcept {
	DEBUG_MSG("initiated for process {} (pid {})", name, pid);
	thread_active = true;

	const uint buffer_size = 512;
	char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

	try {
		while (!must_stop && std::fgets(buffer, buffer_size -1, f_read) != NULL) {
			if (debug_out) {
				std::string aux = str_replace(buffer, '\n', ' ');
				DEBUG_OUT(true, "line read: {}", aux);
			}
			handler(buffer);
		}
	} catch(std::exception& e) {
		DEBUG_MSG("exception received: {}", e.what());
		thread_exception = std::current_exception();
	}
	thread_active = false;
	DEBUG_MSG("finish");
}

bool ProcessController::checkStatus() noexcept {
	//DEBUG_MSG("check status of process {} (pid {})", name, pid);
	int status;

	if (!program_active)
		return false;

	auto w = waitpid(pid, &status, WNOHANG);
	if (w == 0)
		return true;
	if (w == -1) {
		program_active = false;
		spdlog::critical("waitpid error for process {} (pid {})", name, pid);
		std::raise(SIGTERM);
	}
	if (WIFEXITED(status)) {
		exit_code = WEXITSTATUS(status);
		program_active = false;
		std::string msg = fmt::format("process {} (pid {}) exited, status={}", name, pid, exit_code);
		if (exit_code != 0)
			spdlog::warn(msg);
		else
			DEBUG_MSG("{}", msg);
		return false;
	}
	if (WIFSIGNALED(status)) {
		signal = WTERMSIG(status);
		program_active = false;
		spdlog::warn("process {} (pid {}) killed by signal {}", name, pid, signal);
		return false;
	}
	if (WIFSTOPPED(status)) {
		signal = WSTOPSIG(status);
		program_active = false;
		spdlog::warn("process {} (pid {}) stopped by signal {}", name, pid, signal);
		return false;
	}
	program_active = (!WIFEXITED(status) && !WIFSIGNALED(status));
	return program_active;
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

