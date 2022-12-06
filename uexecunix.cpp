#include "uexecunix.hpp"
#include <iostream>
#include <csignal>
#include "uexec.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>


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
}

int uexec::InternalUnix::initUnix(char* const* args, uint32_t size, ScriptRunner* ctx) noexcept
{
	ctx->pid = fork();
	if (ctx->pid != -1)
	{
		currentpid = ctx->pid;
		if (ctx->pid == 0)
		{
			close(ctx->pipefd[0]);

			dup2(ctx->pipefd[1], STDOUT_FILENO);
			dup2(ctx->pipefd[1], STDERR_FILENO);

			close(ctx->pipefd[1]);

			execvp(args[0], args);
			wait(&currentpid);
		}
		else
		{
			ctx->bCanUpdate = true;
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

void uexec::InternalUnix::updateUnix(bool bFirst, ScriptRunner* ctx) noexcept
{
	if (ctx->pid > 0)
	{
		std::vector<char> buf;
		buf.reserve(ctx->bufferSize);
		if (bFirst)
			close(ctx->pipefd[1]);
		if (read(ctx->pipefd[0], buf.data(), sizeof(buf.data())) != 0)
		{
			uexecstring tmp = buf.data();
			tmp.shrink_to_fit();
			ctx->stringBuffer += tmp;
			uexecstring accumulate;
			for (auto& a : ctx->stringBuffer)
			{
				if (a == '\n')
				{
					ctx->lineBuffer.push_back(accumulate);
					accumulate.clear();
				}
				else
					accumulate += a;
			}
		}
	}
}

void uexec::InternalUnix::destroyForReuseUnix(ScriptRunner* ctx) noexcept
{
	ctx->pid = -1;
	ctx->pipefd[0] = -1;
	ctx->pipefd[1] = -1;
}

bool uexec::InternalUnix::finishedUnix(const ScriptRunner* const ctx) noexcept
{
	return (ctx->bCanUpdate && currentpid == -1 ? true : false);
}

void uexec::InternalUnix::terminateUnix(ScriptRunner* ctx) noexcept
{
	kill(currentpid, SIGTERM);
	wait(&currentpid);
}
#else
int uexec::InternalUnix::execandwaitunix(char* const* command) noexcept
{
	return -1;
}

int uexec::InternalUnix::initUnix(char* const* args, uint32_t size, ScriptRunner* ctx) noexcept
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
#endif