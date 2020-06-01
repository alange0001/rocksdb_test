
#include "process.h"

#include <stdexcept>

#include <cstdarg>
#include <csignal>
#include <sys/wait.h>

#include "util.h"

using std::string;
using std::function;
using std::chrono::milliseconds;
using std::exception;
using std::runtime_error;
using fmt::format;

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

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
			throw runtime_error("select call error");
		if (std::feof(file))
			return false;
		if (std::ferror(file))
			throw runtime_error("file error");

		std::this_thread::sleep_for(milliseconds(interval));
	}

	return false;
}

string command_output(const char* cmd, bool debug_out) {
	string ret;
	const uint buffer_size = 512;
	char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

	DEBUG_MSG("command: {}", cmd);

	std::FILE* f = popen(cmd, "r");
	if (f == NULL)
		throw runtime_error(format("error executing command \"{}\"", cmd));

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
		throw runtime_error(format("command \"{}\" returned error {}", cmd, exit_code ));

	return ret;
}

vector<pid_t> get_children(pid_t parent_pid) {
	vector<pid_t> ret;
	DEBUG_MSG("parent pid: {}", parent_pid);
	try {
		string cmd = format(
			"getcpid() {{                       \n"
			"    cpids=$(pgrep -P $1|xargs)     \n"
			"    for cpid in $cpids;            \n"
			"    do                             \n"
			"        echo \"$cpid\"             \n"
			"        getcpid $cpid              \n"
			"    done                           \n"
			"}}                                 \n"
			"getcpid {} |xargs"
			,parent_pid);
		auto children = command_output(cmd.c_str());
		DEBUG_MSG("children: {}", children);
		auto pids = split_str(children, " ");
		for (auto i: pids) {
			if (i.length() == 0) continue;
			try {
				pid_t pid = parseUint64(i, true, 0, format("error parsing child pid (value={})", i).c_str());
				ret.push_back(pid);
			} catch (const std::exception& e) {
				spdlog::error("ERROR: {}", e.what());
			}
		}
	} catch (const std::exception& e) {
		spdlog::error("ERROR: {}", e.what());
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ProcessController::"

ProcessController::ProcessController(const char* name_, const char* cmd,
		function<void(const char*)> handler_stdout_, function<void(const char*)> handler_stderr_,
		bool debug_out_)
: name(name_), handler_stdout(handler_stdout_), handler_stderr(handler_stderr_), debug_out(debug_out_)
{
	DEBUG_MSG("constructor. Process {}", name);

	pid_t child_pid;
	int pipe_stdin[2];
	pipe(pipe_stdin);
	DEBUG_MSG("pipe_stdin=({}, {})", pipe_stdin[0], pipe_stdin[1]);
	int pipe_stdout[2];
	pipe(pipe_stdout);
	DEBUG_MSG("pipe_stdout=({}, {})", pipe_stdout[0], pipe_stdout[1]);
	int pipe_stderr[2];
	pipe(pipe_stderr);
	DEBUG_MSG("pipe_stderr=({}, {})", pipe_stderr[0], pipe_stderr[1]);

	if ((child_pid = fork()) == -1)
		throw runtime_error(format("fork error on process {}", name));

	if (child_pid == 0) { //// child process ////
		//DEBUG_MSG("child process initiated");
		close(pipe_stdin[1]);
		dup2(pipe_stdin[0], STDIN_FILENO);
		close(pipe_stdout[0]);
		dup2(pipe_stdout[1], STDOUT_FILENO);
		close(pipe_stderr[0]);
		dup2(pipe_stderr[1], STDERR_FILENO);

		execl("/bin/bash", "/bin/bash", "-c", cmd, (char*)NULL);
		exit(EXIT_FAILURE);
	}

	program_active = true;

	DEBUG_MSG("child pid={}", child_pid);
	pid   = child_pid;
	close(pipe_stdin[0]);
	if ((f_stdin  = fdopen(pipe_stdin[1], "w")) == NULL)
		throw runtime_error(format("fdopen (pipe_stdin) error on process {}", name));
	close(pipe_stdout[1]);
	if ((f_stdout = fdopen(pipe_stdout[0], "r")) == NULL)
		throw runtime_error(format("fdopen (pipe_stdout) error on process {}", name));
	close(pipe_stderr[1]);
	if ((f_stderr = fdopen(pipe_stderr[0], "r")) == NULL)
		throw runtime_error(format("fdopen (pipe_stderr) error on process {}", name));

	thread_stdout_active = true;
	thread_stdout = std::thread( [this]{this->threadStdout();} );
	thread_stderr_active = true;
	thread_stderr = std::thread( [this]{this->threadStderr();} );
	DEBUG_MSG("constructor finished", name);
}

ProcessController::~ProcessController() {
	DEBUG_MSG("destructor");

	must_stop = true;

	if (checkStatus()) {
		std::this_thread::sleep_for(milliseconds(100));
		if (checkStatus()) {
			spdlog::warn("process {} (pid {}) still active. kill it", name, pid);
			auto children = get_children(pid);
			for (auto i: children) {
				spdlog::warn("child (pid {}) of process {} (pid {}) still active. kill it", i, name, pid);
				kill(i, SIGTERM);
			}
			kill(pid, SIGTERM);
		}
	}

	auto status_f_stdin = std::fclose(f_stdin);
	auto status_f_stdout = std::fclose(f_stdout);
	auto status_f_stderr = std::fclose(f_stderr);
	DEBUG_MSG("status_f_stdin={}, status_f_stdout={}, status_f_stderr={}", status_f_stdin, status_f_stdout, status_f_stderr);

	if (thread_stdout.joinable())
		thread_stdout.join();
	if (thread_stderr.joinable())
		thread_stderr.join();

	DEBUG_MSG("destructor finished");
}

bool ProcessController::isActive(bool throwexcept) {
	if (thread_exception) {
		if (throwexcept)
			std::rethrow_exception(thread_exception);
		else {
			try { std::rethrow_exception(thread_exception); }
			catch (exception& e) {
				spdlog::error("thread exception of program {}: {}", name, e.what());
			}
		}
	}
	bool aux_status = checkStatus();
	if (throwexcept && !aux_status) {
		if (exit_code != 0)
			throw runtime_error(format("program {} exit code {}", name, exit_code));
		if (signal != 0)
			throw runtime_error(format("program {} exit with signal {}", name, signal));
	}
	return thread_stdout_active && thread_stderr_active && aux_status;
}

bool ProcessController::puts(const string value) noexcept {
	DEBUG_MSG("write: {}", value);
	if (!checkStatus()) {
		spdlog::error("puts failed. Process {} is not active", name);
		return false;
	}
	if (std::fputs(value.c_str(), f_stdin) == EOF || std::fflush(f_stdin) == EOF) {
		spdlog::error("fputs/fflush error for process {}", name);
		return false;
	}
	return true;
}

void ProcessController::threadStdout() noexcept {
	DEBUG_MSG("initiated for process {} (pid {})", name, pid);
	thread_stdout_active = true;

	const uint buffer_size = 1024;
	char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

	try {
		while (!must_stop && std::fgets(buffer, buffer_size -1, f_stdout) != NULL) {
			if (debug_out) {
				string aux = str_replace(buffer, '\n', ' ');
				DEBUG_OUT(true, "stdout line: {}", aux);
			}
			handler_stdout(buffer);
		}
	} catch(exception& e) {
		DEBUG_MSG("exception received: {}", e.what());
		thread_exception = std::current_exception();
	}
	thread_stdout_active = false;
	DEBUG_MSG("finish");
}

void ProcessController::threadStderr() noexcept {
	DEBUG_MSG("initiated for process {} (pid {})", name, pid);
	thread_stderr_active = true;

	const uint buffer_size = 1024;
	char buffer[buffer_size]; buffer[0] = '\0'; buffer[buffer_size -1] = '\0';

	try {
		while (!must_stop && std::fgets(buffer, buffer_size -1, f_stderr) != NULL) {
			if (debug_out) {
				string aux = str_replace(buffer, '\n', ' ');
				DEBUG_OUT(true, "stderr line: {}", aux);
			}
			handler_stderr(buffer);
		}
	} catch(exception& e) {
		DEBUG_MSG("exception received: {}", e.what());
		thread_exception = std::current_exception();
	}
	thread_stderr_active = false;
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
		string msg = format("process {} (pid {}) exited, status={}", name, pid, exit_code);
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

