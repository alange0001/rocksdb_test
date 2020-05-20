
#pragma once

#include <string>
#include <thread>
#include <functional>

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

bool monitor_fgets (char* buffer, int buffer_size, std::FILE* file, bool* stop, uint64_t interval=300);

std::string command_output(const char* cmd, bool debug_out=false);

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ "ProcessController::"

class ProcessController {
	std::string name;
	bool        debug_out;

	bool must_stop      = false;
	bool program_active = false;

	pid_t        pid     = 0;
	std::FILE*   f_stdin  = nullptr;
	std::FILE*   f_stdout = nullptr;
	std::FILE*   f_stderr = nullptr;

	std::thread        thread_stdout;
	bool               thread_stdout_active  = false;
	std::thread        thread_stderr;
	bool               thread_stderr_active  = false;
	std::exception_ptr thread_exception;

	std::function<void(const char*)> handler_stdout;
	std::function<void(const char*)> handler_stderr;

	void threadStdout() noexcept;
	void threadStderr() noexcept;
	bool checkStatus() noexcept;

	public: //---------------------------------------------------------------------
	ProcessController(const char* name_, const char* cmd,
			std::function<void(const char*)> handler_stdout_=ProcessController::default_stdout_handler,
			std::function<void(const char*)> handler_stderr_=ProcessController::default_stderr_handler,
			bool debug_out_=false);
	~ProcessController();

	bool puts(const std::string value) noexcept;

	bool isActive(bool throwexcept=false);
	int  exit_code      = 0;
	int  signal         = 0;

	static void default_stderr_handler(const char* v) { std::fputs(v, stderr); }
	static void default_stdout_handler(const char* v) { std::fputs(v, stdout); }
};
