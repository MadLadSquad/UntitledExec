#include "uexecunix.hpp"
#include "uexec.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <functional>

namespace uexec
{
    // This function basically closes the first end of the pipe, duplicates it to the second and closes the second to
    // the child process so that the parent can use it
    void initDescriptors(int* fd, const int file, int fclose, int sclose) noexcept
    {
        close(fd[fclose]);
        dup2(fd[sclose], file);
        close(fd[sclose]);
    }

    void initPipes(int* fd, bool& bOpen) noexcept
    {
        if (bOpen)
            if (pipe(fd) < 0)
                bOpen = false;
    }

	template<typename T>
	T execonparent(ScriptRunner* ctx, const std::function<T(ScriptRunner*)>& func)
	{
		bool bFailedCheck = false;
		for (auto& pid : uexec_internal_pids_do_not_touch)
			if (pid == 0)
				bFailedCheck = true;
		if (!bFailedCheck)
			return func(ctx);
		else
			while (true); // This is here so that we can completely block the event
	}
}


int uexec::InternalUnix::execandwaitunix(char* const* command) noexcept
{
	auto pid = fork();
	if (pid != -1)
	{
		if (pid == 0)
			execvp(command[0], command);
		else
			wait(&pid);
	}
	else
		return -1;
	return 0;
}

int uexec::InternalUnix::initUnix(char* const* args, ScriptRunner* ctx) noexcept
{
	return execonparent<int>(ctx, [&](ScriptRunner* ctx) -> int
	{
		initPipes(ctx->data.pipefdSTDOUT, ctx->data.stdoutOpen);
		initPipes(ctx->data.pipefdSTDERR, ctx->data.stderrOpen);
		initPipes(ctx->data.pipefdSTDIN, ctx->data.stdinOpen);

		ctx->data.pidpos = uexec_internal_pids_do_not_touch.size();
		uexec_internal_pids_do_not_touch.push_back(fork());
		ctx->data.pidp = &uexec_internal_pids_do_not_touch.back();

		auto pid = *ctx->data.pidp;
		if (pid != -1)
		{
			if (pid == 0)
			{
				if (ctx->data.stdoutOpen)
					initDescriptors(ctx->data.pipefdSTDOUT, STDOUT_FILENO, 0, 1);
				if (ctx->data.stderrOpen)
					initDescriptors(ctx->data.pipefdSTDERR, STDERR_FILENO, 0, 1);
				if (ctx->data.stdinOpen)
					initDescriptors(ctx->data.pipefdSTDIN, STDIN_FILENO, 1, 0);

				execvp(args[0], args);
				wait(&pid);
			}
			else
			{
				if (ctx->data.stdoutOpen)
					close(ctx->data.pipefdSTDOUT[1]);
				if (ctx->data.stderrOpen)
					close(ctx->data.pipefdSTDERR[1]);
				if (ctx->data.stdinOpen)
					close(ctx->data.pipefdSTDIN[0]);

				ctx->data.bCanUpdate = true;
				struct sigaction actiondt{};
				// Signal handler for SIGCHLD, basically if the child sends this its execution has ended, so we need
				// to destroy it. However, you might notice that we don't modify the internal pid manager to remove the
				// useless PID. That's because if we did that there is no way for the instance that owns the PID to know
				// if it has died. Instead, the user himself has to handle destruction. This should be achieved by
				// continually querying "finished" then calling "destroyForReuse" on fail
				actiondt.sa_sigaction = [](int signal, siginfo_t* info, void* next) -> void {
					for (int& a : uexec_internal_pids_do_not_touch)
					{
						if (a == info->si_pid)
						{
							wait(&a);
							a = -1;
							break;
						}
					}
				};
				actiondt.sa_flags = SA_SIGINFO;
				sigaction(SIGCHLD, &actiondt, nullptr);
			}
		}
		else
			return -1;
		return 0;
	});
}

void uexec::InternalUnix::destroyForReuseUnix(ScriptRunner* ctx) noexcept
{
	ctx->data.pipefdSTDOUT[0] = -1;
	ctx->data.pipefdSTDOUT[1] = -1;
    ctx->data.pipefdSTDERR[0] = -1;
    ctx->data.pipefdSTDERR[1] = -1;
    ctx->data.pipefdSTDIN[0] = -1;
    ctx->data.pipefdSTDIN[1] = -1;

	// Assign address to null and erase the pid from the pid manager array
	ctx->data.pidp = nullptr;

	return execonparent<void>(ctx, [](ScriptRunner* ctx) -> void {
		uexec_internal_pids_do_not_touch.erase(uexec_internal_pids_do_not_touch.begin() + static_cast<long>(ctx->data.pidpos));
	});
}

bool uexec::InternalUnix::finishedUnix(const ScriptRunner* const ctx) noexcept
{
	return (ctx->data.bCanUpdate && *ctx->data.pidp == -1 ? true : false);
}

void uexec::InternalUnix::terminateUnix(ScriptRunner* ctx) noexcept
{
	return execonparent<void>(ctx, [](ScriptRunner* ctx) -> void {
		kill(*ctx->data.pidp, SIGTERM);
		wait(ctx->data.pidp);
		*ctx->data.pidp = -1;
	});
}

bool uexec::InternalUnix::readUnix(ScriptRunner* ctx, int* pipe, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
	return execonparent<bool>(ctx, [&](ScriptRunner* ctx) -> bool {
		bytesRead = read(pipe[0], buffer.data(), size);
		return bytesRead;
	});
}

bool uexec::InternalUnix::writeUnix(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	return execonparent<bool>(ctx, [&](ScriptRunner* ctx) -> bool {
		bytesWritten = write(ctx->data.pipefdSTDIN[0], buffer.data(), size);
		return bytesWritten;
	});
}
#else
int uexec::InternalUnix::execandwaitunix(char* const* command) noexcept
{
	return -1;
}

int uexec::InternalUnix::initUnix(char* const* args, ScriptRunner* ctx) noexcept
{
	return -1;
}

void uexec::InternalUnix::destroyForReuseUnix(ScriptRunner* ctx) noexcept
{
}

bool uexec::InternalUnix::finishedUnix(const ScriptRunner* const ctx) noexcept
{
	return false;
}

void uexec::InternalUnix::terminateUnix(ScriptRunner* ctx) noexcept
{
}

bool uexec::InternalUnix::readUnix(ScriptRunner* ctx, int* pipe, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
	return false;
}

bool uexec::InternalUnix::writeUnix(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	return false;
}
#endif
