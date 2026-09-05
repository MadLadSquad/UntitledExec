#include "uexecunix.hpp"
#include "uexec.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>

namespace uexec
{
    // This function basically closes the first end of the pipe, duplicates it to the second and closes the second to
    // the child process so that the parent can use it
    static void initDescriptors(int* fd, const int file, int fclose, int sclose) noexcept
    {
        close(fd[fclose]);
        dup2(fd[sclose], file);
        close(fd[sclose]);
    }

    // Closes a descriptor if it is still open and marks it invalid afterwards, so that a second
    // cleanup pass cannot close a descriptor number that has since been handed out to something else
    static void closeEnd(int& fd) noexcept
    {
        if (fd >= 0)
            close(fd);
        fd = -1;
    }

    static void initPipes(int* fd, bool& bOpen) noexcept
    {
        if (bOpen && pipe(fd) == 0)
            return;

        // Either the caller did not ask for this pipe or pipe() failed. Mark both ends invalid so
        // the cleanup path skips them: a zero-initialised RunnerData leaves them at 0, and closing
        // descriptor 0 would take out the process's own stdin
        bOpen = false;
        fd[0] = -1;
        fd[1] = -1;
    }

    // The SIGCHLD handler walks the pid manager, so every structural change to that list has to run
    // with the signal blocked - a handler firing halfway through a push_back or an erase would
    // traverse a half-linked node. Returns the mask to hand back to restoreChildSignal
    static sigset_t blockChildSignal() noexcept
    {
        sigset_t mask;
        sigset_t previous;

        sigemptyset(&mask);
        sigemptyset(&previous);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &previous);
        return previous;
    }

    static void restoreChildSignal(const sigset_t& previous) noexcept
    {
        sigprocmask(SIG_SETMASK, &previous, nullptr);
    }

    // Whatever SIGCHLD disposition the host application had before the library installed its own.
    // Both have static storage duration, so they start zero-initialised: sa_handler compares equal
    // to SIG_DFL and the chaining below correctly does nothing if the handler is never installed
    static struct sigaction previousChildAction;
    static bool bChildActionInstalled = false;

    static void childSignalHandler(int sig, siginfo_t* info, void* signalContext) noexcept
    {
        // SIGCHLD is not a queued signal: several children dying at once collapse into a single
        // delivery, so every slot has to be polled rather than only the one named by si_pid.
        // Reaping by explicit pid (instead of wait()/waitpid(-1)) also keeps the library from
        // stealing a child that belongs to the host application
        const int savedErrno = errno;
        for (pid_t& a : uexec_internal_pids_do_not_touch)
            if (a > 0 && waitpid(a, nullptr, WNOHANG) == a)
                a = -1;
        errno = savedErrno;

        // The pid is deliberately left in the list rather than erased: that is the only way for the
        // instance owning it to notice the death through finished(). Removal happens in
        // destroy()/destroyForReuse()

        // Chain to the application's own handler so installing ours does not silently swallow its
        // child notifications
        if ((previousChildAction.sa_flags & SA_SIGINFO) != 0)
        {
            if (previousChildAction.sa_sigaction != nullptr)
                previousChildAction.sa_sigaction(sig, info, signalContext);
        }
        else if (previousChildAction.sa_handler != SIG_DFL && previousChildAction.sa_handler != SIG_IGN)
            previousChildAction.sa_handler(sig);
    }

    // Installed once for the whole process. Re-installing on every init() would overwrite the saved
    // disposition with our own handler and turn the chaining above into infinite recursion
    static void installChildHandler() noexcept
    {
        if (bChildActionInstalled)
            return;

        struct sigaction action{};
        action.sa_sigaction = childSignalHandler;
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGCHLD);
        // SA_RESTART stops a blocking read()/write() in the parent from failing with EINTR every
        // time some unrelated child of the application exits
        action.sa_flags = SA_SIGINFO | SA_RESTART;

        if (sigaction(SIGCHLD, &action, &previousChildAction) == 0)
            bChildActionInstalled = true;
    }

	// Takes the callable as a template parameter rather than a std::function: the latter heap
	// allocates on every read/write, and a failed allocation would throw out of a noexcept function
	template<typename T, typename Func>
	static T execonparent(ScriptRunner* ctx, Func&& func)
	{
		bool bFailedCheck = false;
		for (const auto& pid : uexec_internal_pids_do_not_touch)
			if (pid == 0)
				bFailedCheck = true;
		if (!bFailedCheck)
			return func(ctx);

		// This is here so that we can completely block the event. pause() rather than a bare
		// `while (true);` because a loop with no observable behaviour may be assumed to terminate,
		// which would let the compiler drop the guard and fall straight through it
		while (true)
			pause();
	}
}


int uexec::InternalUnix::execandwaitunix(char* const* command) noexcept
{
	const pid_t pid = fork();
	if (pid == -1)
		return -1;

	if (pid == 0)
	{
		execvp(command[0], command);
		// execvp only returns on failure - the child must not fall through into the
		// caller's code, otherwise it would run as a duplicate of the parent process
		_exit(127);
	}

	// waitpid on this pid rather than wait(): a bare wait() reaps whichever child happens to be
	// ready, which may well be a ScriptRunner's child. That runner's slot would then never be
	// marked dead and its finished() would never return true
	while (waitpid(pid, nullptr, 0) == -1 && errno == EINTR);
	return 0;
}

int uexec::InternalUnix::initUnix(char* const* args, ScriptRunner* ctx) noexcept
{
	return execonparent<int>(ctx, [&](ScriptRunner* context) -> int
	{
		initPipes(context->data.pipefdSTDOUT, context->data.stdoutOpen);
		initPipes(context->data.pipefdSTDERR, context->data.stderrOpen);
		initPipes(context->data.pipefdSTDIN, context->data.stdinOpen);

		// The handler has to exist before the fork. Installing it afterwards leaves a window in
		// which a fast-exiting child's SIGCHLD is dropped by the default disposition, and that
		// runner's finished() then never returns true
		installChildHandler();

		// The slot is reserved before the fork so that the child never has to allocate: malloc
		// between fork() and execvp() is not async-signal-safe and deadlocks outright if another
		// thread happened to hold the allocator lock at the moment of the fork
		const sigset_t previousMask = blockChildSignal();
		uexec_internal_pids_do_not_touch.push_back(-1);
		// The list guarantees this reference stays valid even as other runners are added/removed
		context->data.pidp = &uexec_internal_pids_do_not_touch.back();

		const pid_t pid = fork();
		if (pid == -1)
		{
			// Nothing else can have touched the list while SIGCHLD was blocked, so the slot we
			// reserved is still the back one
			uexec_internal_pids_do_not_touch.pop_back();
			context->data.pidp = nullptr;
			restoreChildSignal(previousMask);
			return -1;
		}

		// 0 in the child, the child's pid in the parent. This writes into the slot reserved above,
		// so neither process allocates here
		*context->data.pidp = pid;

		if (pid == 0)
		{
			// The child must not carry a blocked SIGCHLD across the exec
			restoreChildSignal(previousMask);

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

		// The child owns these ends now. closeEnd invalidates them as it goes, so the
		// cleanup in closeDescriptorsUnix cannot close the same descriptor number twice
		closeEnd(context->data.pipefdSTDOUT[1]);
		closeEnd(context->data.pipefdSTDERR[1]);
		closeEnd(context->data.pipefdSTDIN[0]);

		context->data.bCanUpdate = true;
		restoreChildSignal(previousMask);
		return 0;
	});
}

void uexec::InternalUnix::closeDescriptorsUnix(ScriptRunner* ctx) noexcept
{
	// Only the parent's ends are still open by now: the read ends of STDOUT/STDERR and the write end
	// of STDIN. The others were closed and invalidated right after the fork. This is idempotent, so
	// both destroy() and destroyForReuse() can call it without risking a double close
	closeEnd(ctx->data.pipefdSTDOUT[0]);
	closeEnd(ctx->data.pipefdSTDOUT[1]);
	closeEnd(ctx->data.pipefdSTDERR[0]);
	closeEnd(ctx->data.pipefdSTDERR[1]);
	closeEnd(ctx->data.pipefdSTDIN[0]);
	closeEnd(ctx->data.pipefdSTDIN[1]);
}

void uexec::InternalUnix::releasePidSlotUnix(ScriptRunner* ctx) noexcept
{
	// Erase this runner's slot from the pid manager by matching its stable address, then null
	// the pointer. Indexing by position would be wrong here: erasing a slot must not disturb the
	// slots (and therefore the pidp pointers) owned by any other live runner.
	// destroy() ends up here as well as destroyForReuse() - leaving it to the latter alone meant a
	// plain init()/destroy() cycle leaked one slot per run for the lifetime of the process
	pid_t* const target = ctx->data.pidp;
	if (target == nullptr)
		return;
	ctx->data.pidp = nullptr;

	return execonparent<void>(ctx, [target](ScriptRunner*) -> void {
		const sigset_t previousMask = blockChildSignal();
		for (auto it = uexec_internal_pids_do_not_touch.begin(); it != uexec_internal_pids_do_not_touch.end(); ++it)
		{
			if (&*it == target)
			{
				uexec_internal_pids_do_not_touch.erase(it);
				break;
			}
		}
		restoreChildSignal(previousMask);
	});
}

void uexec::InternalUnix::destroyForReuseUnix(ScriptRunner* ctx) noexcept
{
	closeDescriptorsUnix(ctx);
	releasePidSlotUnix(ctx);
}

bool uexec::InternalUnix::finishedUnix(const ScriptRunner* const ctx) noexcept
{
	// destroyForReuse nulls pidp, so it has to be checked before the slot is read
	return ctx->data.bCanUpdate && ctx->data.pidp != nullptr && *ctx->data.pidp == -1;
}

void uexec::InternalUnix::terminateUnix(ScriptRunner* ctx) noexcept
{
	// A runner that was never started, or that has already been torn down, owns no slot to signal
	if (ctx->data.pidp == nullptr)
		return;

	return execonparent<void>(ctx, [](ScriptRunner* ctx) -> void {
		const pid_t pid = *ctx->data.pidp;
		// kill() reads every pid <= 0 as a process-group broadcast: 0 is our own group and -1 is
		// every process this user is allowed to signal. A slot holding -1 - a child the SIGCHLD
		// handler has already reaped, or a fork that failed - must never reach it
		if (pid <= 0)
			return;

		kill(pid, SIGTERM);
		// waitpid on this specific pid rather than wait(): the SIGCHLD handler may have reaped the
		// child already, in which case this fails with ECHILD instead of blocking on an unrelated one
		while (waitpid(pid, nullptr, 0) == -1 && errno == EINTR);
		*ctx->data.pidp = -1;
	});
}

bool uexec::InternalUnix::readUnix(ScriptRunner* ctx, int* pipe, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
	return execonparent<bool>(ctx, [&](ScriptRunner*) -> bool {
		bytesRead = 0;

		// The caller's size is a request, not a promise about the buffer. Reading more than the
		// string actually holds writes straight past the end of its storage
		const size_t maxRead = size < buffer.size() ? size : buffer.size();
		if (pipe[0] < 0 || maxRead == 0)
			return false;

		// read() hands back a signed count. Assigning -1 into bytesRead surfaces it as SIZE_MAX,
		// and being non-zero it used to be reported to the caller as a successful read
		const ssize_t result = read(pipe[0], buffer.data(), maxRead);
		if (result <= 0)
			return false; // A negative result is an error, zero is end of stream

		bytesRead = static_cast<size_t>(result);
		return true;
	});
}

bool uexec::InternalUnix::writeUnix(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	return execonparent<bool>(ctx, [&](ScriptRunner* context) -> bool {
		bytesWritten = 0;

		// Clamp to what the buffer actually holds. Writing the caller's requested size out of a
		// shorter string reads past its storage and hands that memory to the child process
		const size_t maxWrite = size < buffer.size() ? size : buffer.size();
		if (context->data.pipefdSTDIN[1] < 0 || maxWrite == 0)
			return false;

		// The parent writes to the write end [1]; the read end [0] was closed after the fork
		const ssize_t result = write(context->data.pipefdSTDIN[1], buffer.data(), maxWrite);
		if (result <= 0)
			return false;

		bytesWritten = static_cast<size_t>(result);
		return true;
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

void uexec::InternalUnix::closeDescriptorsUnix(ScriptRunner* ctx) noexcept
{
}

void uexec::InternalUnix::releasePidSlotUnix(ScriptRunner* ctx) noexcept
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
