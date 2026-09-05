#include "uexec.hpp"
#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
#endif

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

int uexec::ScriptRunner::init(char* const* args, const bool bOpenStderrPipe, const bool bOpenStdoutPipe, const bool bOpenStdinPipe) noexcept
{
	// A runner that is already driving a process has to be torn down first. Initialising over the
	// top of one orphans the running child along with its pipes and its slot in the pid manager,
	// none of which anything can reach afterwards. A fresh or destroyed runner has bValid false,
	// and destroyForReuse() sets bCanRestart, so only the live case is rejected here
	if (data.bValid && !data.bCanRestart)
		return -1;

	data.bCanRestart = false;
	data.bValid = true;

	data.stderrOpen = bOpenStderrPipe;
	data.stdoutOpen = bOpenStdoutPipe;
	data.stdinOpen = bOpenStdinPipe;

#ifdef _WIN32
	data.bFinished = false;
	// Windows drives completion detection through update(), which is gated on bCanUpdate
	data.bCanUpdate = true;
	return InternalWindows::initWindows(args, this);
#else
	// The Unix parent branch flips this on once the child has been forked successfully
	data.bCanUpdate = false;
	return InternalUnix::initUnix(args, this);
#endif
}

void uexec::ScriptRunner::update() noexcept
{
	if (data.bCanUpdate)
		InternalWindows::updateWindows(this);
}

void uexec::ScriptRunner::destroy() noexcept
{
	if (!finished())
		terminate();

	// destroy() is the terminal call for a runner that will not be reused, so everything the run
	// still owns has to be released here too: the parent's pipe ends, and on Unix the runner's slot
	// in the global pid manager. Leaving either to destroyForReuse() meant a plain init()/destroy()
	// cycle leaked both for the lifetime of the process
	InternalWindows::closeDescriptorsWindows(this);
	InternalUnix::closeDescriptorsUnix(this);
	InternalUnix::releasePidSlotUnix(this);

	data.bValid = false;
}

bool uexec::ScriptRunner::valid() const noexcept
{
	return data.bValid;
}

void uexec::ScriptRunner::destroyForReuse() noexcept
{
	if (!finished())
		destroy();

	data.bCanRestart = true;
	data.bCanUpdate = false;
	data.bValid = true;

	InternalWindows::destroyForReuseWindows(this);
	InternalUnix::destroyForReuseUnix(this);
}

bool uexec::ScriptRunner::finished() const noexcept
{
#ifdef _WIN32
	return InternalWindows::finishedWindows(this);
#else
	return InternalUnix::finishedUnix(this);
#endif
}

bool uexec::ScriptRunner::startable() const noexcept
{
    return data.bCanRestart;
}

void uexec::ScriptRunner::terminate() noexcept
{
	InternalWindows::terminateWindows(this);
	InternalUnix::terminateUnix(this);
}

bool uexec::ScriptRunner::readSTDOUT(uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
#ifdef _WIN32
	return InternalWindows::readWindows(this, data.stdoutRead, buffer, size, bytesRead);
#else
	return InternalUnix::readUnix(this, data.pipefdSTDOUT, buffer, size, bytesRead);
#endif
}

bool uexec::ScriptRunner::readSTDERR(uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
#ifdef _WIN32
	return InternalWindows::readWindows(this, data.stderrRead, buffer, size, bytesRead);
#else
	return InternalUnix::readUnix(this, data.pipefdSTDERR, buffer, size, bytesRead);
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
