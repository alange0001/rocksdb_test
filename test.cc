
#include <chrono>

//#include <stdio.h>
#include <csignal>
#include <sys/wait.h>

#include "util.h"


int64_t ParetoCDFInversion(double u, double theta, double k, double sigma) {
	double ret;
	if (k == 0.0) {
		ret = theta - sigma * std::log(u);
	} else {
		ret = theta + sigma * (std::pow(u, -1 * k) - 1) / k;
	}
	return static_cast<int64_t>(ceil(ret));
}

int64_t PowerCdfInversion(double u, double a, double b, double c) {
  double ret;
  ret = std::pow(((u - c) / a), (1 / b));
  return static_cast<int64_t>(ceil(ret));
}



std::FILE * custom_popen(const char* command, char type, pid_t* pid)
{
    pid_t child_pid;
    int fd[2];
    pipe(fd);

    if((child_pid = fork()) == -1)
    {
        perror("fork");
        exit(1);
    }

    /* child process */
    if (child_pid == 0)
    {
        if (type == 'r')
        {
            close(fd[0]);    //Close the READ end of the pipe since the child's fd is write-only
            dup2(fd[1], 1); //Redirect stdout to pipe
        }
        else
        {
            close(fd[1]);    //Close the WRITE end of the pipe since the child's fd is read-only
            dup2(fd[0], 0);   //Redirect stdin to pipe
        }

        setpgid(child_pid, child_pid); //Needed so negative PIDs can kill children of /bin/sh
        execl(command, command, (char*)NULL);
        exit(0);
    }
    else
    {
        printf("child pid %d\n", child_pid);
        if (type == 'r')
        {
            close(fd[1]); //Close the WRITE end of the pipe since parent's fd is read-only
        }
        else
        {
            close(fd[0]); //Close the READ end of the pipe since parent's fd is write-only
        }
    }

    *pid = child_pid;

    if (type == 'r')
    {
        return fdopen(fd[0], "r");
    }

    return fdopen(fd[1], "w");
}

int custom_pclose(FILE * fp, pid_t pid)
{
    int stat;

    fclose(fp);
    while (waitpid(pid, &stat, 0) == -1)
    {
        if (errno != EINTR)
        {
            stat = -1;
            break;
        }
    }

    return stat;
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

std::string command_output(const char* cmd, bool debug_out=false) {
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

class ProcessController {
	std::string name;
	bool        debug_out;

	bool must_stop      = false;
	bool thread_active  = false;
	bool program_active = false;

	pid_t        pid   = 0;
	std::FILE*   f_read  = nullptr;
	std::FILE*   f_write = nullptr;

	std::thread        thread;
	std::exception_ptr thread_exception;

	std::function<void(const char*)> handler;

	public: //---------------------------------------------------------------------
	ProcessController(const char* name_, const char* cmd, std::function<void(const char*)> handler_,
			bool debug_out_=false)
	: name(name_), handler(handler_), debug_out(debug_out_)
	{
		DEBUG_MSG("constructor. Process {}", name);

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

	~ProcessController() {
		DEBUG_MSG("destructor");

		must_stop = true;

	    if (checkStatus()) {
	    	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    	spdlog::error("process {} (pid {}) still active. kill it", name, pid);
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

	bool isActive() {
		if (thread_exception)
			std::rethrow_exception(thread_exception);
		return thread_active && checkStatus();
	}

	bool puts(const std::string value) noexcept {
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

	private: //--------------------------------------------------------------------

	void threadMain() noexcept {
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

	bool checkStatus() noexcept {
	    DEBUG_MSG("check status of process {} (pid {})", name, pid);
		int status;

		if (!program_active)
			return false;

		auto w = waitpid(pid, &status, WNOHANG);
		if (w == 0)
			return true;
		if (w == -1) {
			program_active = false;
			spdlog::error("waitpid error for process {} (pid {})", name, pid);
			return false;
		}
		if (WIFEXITED(status)) {
			program_active = false;
			std::string msg = fmt::format("process {} (pid {}) exited, status={}", name, pid, WEXITSTATUS(status));
			if (WEXITSTATUS(status) != 0)
				spdlog::warn(msg);
			else
				DEBUG_MSG("{}", msg);
			return false;
		}
		if (WIFSIGNALED(status)) {
			program_active = false;
			spdlog::warn("process {} (pid {}) killed by signal {}", name, pid, WTERMSIG(status));
			return false;
		}
		if (WIFSTOPPED(status)) {
			program_active = false;
			spdlog::warn("process {} (pid {}) stopped by signal {}", name, pid, WSTOPSIG(status));
			return false;
		}
		program_active = (!WIFEXITED(status) && !WIFSIGNALED(status));
		return program_active;
	}

};

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

void line_handler(const char* line) {
	DEBUG_MSG("handler line: {}", str_replace(line, '\n', ' '));
}

////////////////////////////////////////////////////////////////////////////////////
#undef __CLASS__
#define __CLASS__ ""

int main() {
	spdlog::set_level(spdlog::level::debug);

	//fmt::print(stdout, "{}", command_output("build/access_time3 --log_level=debug --filename=/mnt/work/tmp/0 --create_file=false --block_size=4 --wait=true 2>&1", true));

	//ProcessController p("test1", "build/access_time3 --log_level=debug --filename=/mnt/work/tmp/0 --create_file=false --block_size=4 --wait=true 2>&1", line_handler, false);
	ProcessController p("test1", "ls -l", line_handler, false);

	uint count=0;
	while (p.isActive()) {
		if (++count >= 5)
			p.puts("stop\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		if (count >= 5)
			p.puts("stop\n");
	}
	DEBUG_MSG("not active"); //*/

	return 0;
}
