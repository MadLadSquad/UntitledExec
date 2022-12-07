#include "uexec.hpp"
#ifdef _WIN32
	#include <windows.h>
	#include <iostream>
#else
	#include <unistd.h>
	#include <sys/wait.h>
#endif
#include <csignal>
#include <chrono>

#include "uexecunix.hpp"
#include "uexecwindows.hpp"

int uexec::execandwait(char* const* command) noexcept
{
#ifdef _WIN32
	return InternalWindows::execandwaitWindows(command);
#else
	return InternalUnix::execandwaitunix(command);
#endif
}

int uexec::ScriptRunner::init(char* const* args, uint32_t size) noexcept
{
    bCanRestart = false;
	stringBuffer.reserve(size);
	bufferSize = size;

#ifdef _WIN32
	return InternalWindows::initWindows(args, size, this);
#else
	return InternalUnix::initUnix(args, size, this);
#endif
}

void uexec::ScriptRunner::update(bool bFirst) noexcept
{
	if (bCanUpdate)
	{	
		InternalWindows::updateWindows(bFirst, this);
		InternalUnix::updateUnix(bFirst, this);
	}
}

void uexec::ScriptRunner::updateBufferSize() noexcept
{
	if (bCanUpdate)
	{
#ifndef _WIN32
		if (pid > 0)
		{
#endif
		stringBuffer.clear();
		stringBuffer.reserve(bufferSize);
#ifndef _WIN32
		}
#endif
	}
}

void uexec::ScriptRunner::destroy() noexcept
{
	if (finished())
		bValid = false;
	else
		terminate();
}

bool uexec::ScriptRunner::valid() const noexcept
{
	return bValid;
}

void uexec::ScriptRunner::destroyForReuse() noexcept
{
	if (finished())
	{
        bCanRestart = true;
		stringBuffer.clear();
		lineBuffer.clear();
		bCanUpdate = false;
		bValid = true;

		InternalWindows::destroyForReuseWindows(this);
		InternalUnix::destroyForReuseUnix(this);
	}
}

bool uexec::ScriptRunner::finished() const noexcept
{
#ifdef _WIN32
	return bFinished;
#else
	return InternalUnix::finishedUnix(this);
#endif
}

std::vector<uexecstring>& uexec::ScriptRunner::data() noexcept
{
	return lineBuffer;
}

bool uexec::ScriptRunner::startable() const noexcept
{
    return bCanRestart;
}

void uexec::ScriptRunner::terminate() noexcept
{
	InternalWindows::terminateWindows(this);
	InternalUnix::terminateUnix(this);
}

bool uexec::ScriptRunner::read(UExecStreams stream, std::string& buffer, size_t size, size_t& bytesRead) noexcept
{
	return false;
}

bool uexec::ScriptRunner::write(UExecStreams stream, std::string& buffer, size_t size, size_t& bytesWritten) noexcept
{
	return false;
}
