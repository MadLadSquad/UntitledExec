#include "uexec.hpp"
#ifdef _WIN32
	#include <windows.h>
	#include <iostream>
#else
	#include <unistd.h>
#endif
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

int uexec::ScriptRunner::init(char* const* args, bool bOpenStderrPipe, bool bOpenStdoutPipe, bool bOpenStdinPipe) noexcept
{
    bCanRestart = false;

	stderrOpen = bOpenStderrPipe;
	stdoutOpen = bOpenStdoutPipe;
	stdinOpen = bOpenStdinPipe;

#ifdef _WIN32
	return InternalWindows::initWindows(args, this);
#else
	return InternalUnix::initUnix(args, this);
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

bool uexec::ScriptRunner::startable() const noexcept
{
    return bCanRestart;
}

void uexec::ScriptRunner::terminate() noexcept
{
	InternalWindows::terminateWindows(this);
	InternalUnix::terminateUnix(this);
}

bool uexec::ScriptRunner::readSTDOUT(uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
#ifdef _WIN32
	return InternalWindows::readWindows(this, stdoutRead, buffer, size, bytesRead);
#else
	return InternalUnix::readUnix(this, pipefdSTDOUT, buffer, size, bytesRead);
#endif
}

bool uexec::ScriptRunner::readSTDERR(uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
#ifdef _WIN32
	return InternalWindows::readWindows(this, stderrRead, buffer, size, bytesRead);
#else
	return InternalUnix::readUnix(this, pipefdSTDERR, buffer, size, bytesRead);
#endif
}

bool uexec::ScriptRunner::write(uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
#ifdef _WIN32
	return InternalWindows::writeWindows(this, buffer, size, bytesWritten);
#else
	return InternalUnix::writeUnix(this, buffer, size, bytesWritten);
#endif
}
