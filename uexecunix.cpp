#include "uexecunix.hpp"
#include "uexec.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>


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
    initPipes(ctx->data.pipefdSTDOUT, ctx->data.stdoutOpen);
    initPipes(ctx->data.pipefdSTDERR, ctx->data.stderrOpen);
    initPipes(ctx->data.pipefdSTDIN, ctx->data.stdinOpen);


	ctx->data.pid = fork();
	if (ctx->data.pid != -1)
	{
		currentpid = ctx->data.pid;
		if (ctx->data.pid == 0)
		{
            if (ctx->data.stdoutOpen)
                initDescriptors(ctx->data.pipefdSTDOUT, STDOUT_FILENO, 0, 1);
            if (ctx->data.stderrOpen)
                initDescriptors(ctx->data.pipefdSTDERR, STDERR_FILENO, 0, 1);
            if (ctx->data.stdinOpen)
                initDescriptors(ctx->data.pipefdSTDIN, STDIN_FILENO, 1, 0);

			execvp(args[0], args);
			wait(&currentpid);
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
			signal(SIGCHLD, [](int sig)
			{
				if (currentpid > 0)
				{
					wait(&currentpid);	// Wait for the process to finish
					currentpid = -1;	// Reset the pid
				}
			});
		}
	}
	else
	{
		return -1;
	}
	return 0;
}

void uexec::InternalUnix::destroyForReuseUnix(ScriptRunner* ctx) noexcept
{
	ctx->data.pid = -1;
	ctx->data.pipefdSTDOUT[0] = -1;
	ctx->data.pipefdSTDOUT[1] = -1;
    ctx->data.pipefdSTDERR[0] = -1;
    ctx->data.pipefdSTDERR[1] = -1;
    ctx->data.pipefdSTDIN[0] = -1;
    ctx->data.pipefdSTDIN[1] = -1;

}

bool uexec::InternalUnix::finishedUnix(const ScriptRunner* const ctx) noexcept
{
	return (ctx->data.bCanUpdate && currentpid == -1 ? true : false);
}

void uexec::InternalUnix::terminateUnix(ScriptRunner* ctx) noexcept
{
	kill(currentpid, SIGTERM);
	wait(&currentpid);
}

bool uexec::InternalUnix::readUnix(ScriptRunner* ctx, int* pipe, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
	bytesRead = read(pipe[0], buffer.data(), size);
    return bytesRead;
}

bool uexec::InternalUnix::writeUnix(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	bytesWritten = write(ctx->data.pipefdSTDIN[0], buffer.data(), size);
    return bytesWritten;
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

void uexec::InternalUnix::updateUnix(bool bFirst, ScriptRunner* ctx) noexcept
{
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