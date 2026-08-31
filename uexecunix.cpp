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
		for (const auto& pid : uexec_internal_pids_do_not_touch)
			if (pid == 0)
				bFailedCheck = true;
		if (!bFailedCheck)
			return func(ctx);
    	while (true); // This is here so that we can completely block the event
	}
}


int uexec::InternalUnix::execandwaitunix(char* const* command) noexcept
{
	auto pid = fork();
	if (pid != -1)
	{
		if (pid == 0)
		{
			execvp(command[0], command);
			// execvp only returns on failure - the child must not fall through into the
			// caller's code, otherwise it would run as a duplicate of the parent process
			_exit(127);
		}
		else
			wait(&pid);
	}
	else
		return -1;
	return 0;
}

int uexec::InternalUnix::initUnix(char* const* args, ScriptRunner* ctx) noexcept
{
	return execonparent<int>(ctx, [&](ScriptRunner* context) -> int
	{
		initPipes(context->data.pipefdSTDOUT, context->data.stdoutOpen);
		initPipes(context->data.pipefdSTDERR, context->data.stderrOpen);
		initPipes(context->data.pipefdSTDIN, context->data.stdinOpen);

		uexec_internal_pids_do_not_touch.push_back(fork());
		// The list guarantees this reference stays valid even as other runners are added/removed
		context->data.pidp = &uexec_internal_pids_do_not_touch.back();

		auto pid = *context->data.pidp;
		if (pid != -1)
		{
			if (pid == 0)
			{
				if (context->data.stdoutOpen)
					initDescriptors(context->data.pipefdSTDOUT, STDOUT_FILENO, 0, 1);
				if (context->data.stderrOpen)
					initDescriptors(context->data.pipefdSTDERR, STDERR_FILENO, 0, 1);
				if (context->data.stdinOpen)
					initDescriptors(context->data.pipefdSTDIN, STDIN_FILENO, 1, 0);

				execvp(args[0], args);
				// execvp only returns on failure - terminate the child immediately so it
				// cannot fall through into the parent's application code
				_exit(127);
			}
			else
			{
				if (context->data.stdoutOpen)
					close(context->data.pipefdSTDOUT[1]);
				if (context->data.stderrOpen)
					close(context->data.pipefdSTDERR[1]);
				if (context->data.stdinOpen)
					close(context->data.pipefdSTDIN[0]);

				context->data.bCanUpdate = true;
				struct sigaction actiondt{};
				// Signal handler for SIGCHLD, basically if the child sends this its execution has ended, so we need
				// to destroy it. However, you might notice that we don't modify the internal pid manager to remove the
				// useless PID. That's because if we did that there is no way for the instance that owns the PID to know
				// if it has died. Instead, the user himself has to handle destruction. This should be achieved by
				// continually querying "finished" then calling "destroyForReuse" on fail
				actiondt.sa_sigaction = [](int, siginfo_t* info, void*) -> void {
					for (pid_t& a : uexec_internal_pids_do_not_touch)
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

	// Erase this runner's slot from the pid manager by matching its stable address, then null
	// the pointer. Indexing by position would be wrong here: erasing a slot must not disturb the
	// slots (and therefore the pidp pointers) owned by any other live runner
	pid_t* const target = ctx->data.pidp;
	ctx->data.pidp = nullptr;

	return execonparent<void>(ctx, [target](ScriptRunner*) -> void {
		for (auto it = uexec_internal_pids_do_not_touch.begin(); it != uexec_internal_pids_do_not_touch.end(); ++it)
		{
			if (&*it == target)
			{
				uexec_internal_pids_do_not_touch.erase(it);
				break;
			}
		}
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
	return execonparent<bool>(ctx, [&](ScriptRunner*) -> bool {
		bytesRead = read(pipe[0], buffer.data(), size);
		return bytesRead;
	});
}

bool uexec::InternalUnix::writeUnix(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	return execonparent<bool>(ctx, [&](ScriptRunner* context) -> bool {
		// The parent writes to the write end [1]; the read end [0] was closed after the fork
		bytesWritten = write(context->data.pipefdSTDIN[1], buffer.data(), size);
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
