#include "uexecwindows.hpp"
#include "uexec.hpp"

#ifdef _WIN32
#include <windows.h>
#include <string>

// Appends a single argument to a command line, quoting/escaping it per the rules used by
// CommandLineToArgvW / the MSVC CRT so that arguments containing spaces, tabs or quotes survive
// intact. Without this, any argument with a space would be split by the child process.
static void appendQuotedArg(std::string& out, const char* arg) noexcept
{
	const std::string s = arg;
	if (!s.empty() && s.find_first_of(" \t\n\v\"") == std::string::npos)
	{
		out += s;
		return;
	}

	out += '"';
	for (size_t i = 0; ; ++i)
	{
		size_t backslashes = 0;
		while (i < s.size() && s[i] == '\\')
		{
			++i;
			++backslashes;
		}

		if (i == s.size())
		{
			// Escape trailing backslashes so they don't escape the closing quote
			out.append(backslashes * 2, '\\');
			break;
		}
		if (s[i] == '"')
		{
			// Escape all backslashes plus the embedded quote
			out.append(backslashes * 2 + 1, '\\');
			out += '"';
		}
		else
		{
			out.append(backslashes, '\\');
			out += s[i];
		}
	}
	out += '"';
}

// Builds a fully quoted command line from filename + args[1..] (args[0] is replaced by the
// resolved filename)
static uexecstring buildCommandLine(const char* filename, char* const* args) noexcept
{
	std::string cmdline;
	appendQuotedArg(cmdline, filename);
	for (size_t i = 1; args[i] != nullptr; i++)
	{
		cmdline += ' ';
		appendQuotedArg(cmdline, args[i]);
	}
	return uexecstring(cmdline.c_str());
}

// Resolves a command name to a full path in out, which must hold at least MAX_PATH bytes.
// SearchPathA is asked for a default extension only when the name does not already carry one: the
// extension used to be sliced out with substr(find_last_of(".")), which throws std::out_of_range
// for any command without a dot ("notepad") - inside a noexcept function, and through an exception
// path the framework forbids outright. A failed lookup is now reported instead of leaving out
// uninitialised for CreateProcess to consume
static bool resolveExecutable(const char* name, char* out) noexcept
{
	out[0] = '\0';
	if (name == nullptr || name[0] == '\0')
		return false;

	const std::string s = name;
	const size_t dot = s.find_last_of('.');
	const size_t separator = s.find_last_of("\\/");
	// A dot only counts as an extension when it belongs to the file name rather than to a directory
	// along the way ("..\\tools\\run")
	const bool bHasExtension = dot != std::string::npos && (separator == std::string::npos || dot > separator);

	LPSTR filepart = nullptr;
	return SearchPathA(nullptr, name, bHasExtension ? nullptr : ".exe", MAX_PATH, out, &filepart) != 0;
}

// Closes a handle if it is still open and marks it invalid, so that repeated teardown cannot close
// a handle value Windows has since recycled for something else
static void closeHandleEnd(HANDLE& handle) noexcept
{
	if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
		CloseHandle(handle);
	handle = nullptr;
}

// Closes every pipe handle the runner owns. Idempotent
static void closePipeHandles(uexec_RunnerData& data) noexcept
{
	closeHandleEnd(data.stderrRead);
	closeHandleEnd(data.stderrWrite);
	closeHandleEnd(data.stdoutRead);
	closeHandleEnd(data.stdoutWrite);
	closeHandleEnd(data.stdinRead);
	closeHandleEnd(data.stdinWrite);
}

// Closes the process and thread handles from a PROCESS_INFORMATION. Idempotent
static void closeProcessHandles(PROCESS_INFORMATION& pif) noexcept
{
	closeHandleEnd(pif.hThread);
	closeHandleEnd(pif.hProcess);
}

int uexec::InternalWindows::execandwaitWindows(char* const* command) noexcept
{
	char filename[MAX_PATH];
	if (!resolveExecutable(command[0], filename))
		return -1;

	uexecstring str = buildCommandLine(filename, command);

	PROCESS_INFORMATION pif;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pif, sizeof(pif));
	si.cb = sizeof(si);

	constexpr DWORD creationFlags = CREATE_NEW_CONSOLE;
	if (!CreateProcess(filename, str.data(), nullptr, nullptr, false, creationFlags, nullptr, nullptr, &si, &pif))
		return -1;

	WaitForSingleObject(pif.hProcess, INFINITE);
	closeProcessHandles(pif);
	return 0;
}

int uexec::InternalWindows::initWindows(char* const* args, ScriptRunner* ctx) noexcept
{
	char filename[MAX_PATH];
	if (!resolveExecutable(args[0], filename))
		return -1;

	uexecstring str = buildCommandLine(filename, args);

	// Start from a clean slate. Anything a previous run left behind would otherwise be leaked here
	// and, worse, its stale handle values would be closed a second time on the next teardown
	closePipeHandles(ctx->data);
	closeProcessHandles(ctx->data.pif);
	ZeroMemory(&ctx->data.pif, sizeof(PROCESS_INFORMATION));

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = true;
	sa.lpSecurityDescriptor = nullptr;


	// Each pipe is created inheritable, then the parent-owned end is marked non-inheritable so the
	// child does not inherit a duplicate of it. Leaving the parent's ends inheritable is the classic
	// MSDN pitfall: the pipe never reports EOF because a live write handle survives in the child.
	if (ctx->data.stderrOpen)
	{
		ctx->data.stderrOpen = CreatePipe(&ctx->data.stderrRead, &ctx->data.stderrWrite, &sa, 0);
		if (ctx->data.stderrOpen)
			SetHandleInformation(ctx->data.stderrRead, HANDLE_FLAG_INHERIT, 0);
	}
	if (ctx->data.stdoutOpen)
	{
		ctx->data.stdoutOpen = CreatePipe(&ctx->data.stdoutRead, &ctx->data.stdoutWrite, &sa, 0);
		if (ctx->data.stdoutOpen)
			SetHandleInformation(ctx->data.stdoutRead, HANDLE_FLAG_INHERIT, 0);
	}
	if (ctx->data.stdinOpen)
	{
		ctx->data.stdinOpen = CreatePipe(&ctx->data.stdinRead, &ctx->data.stdinWrite, &sa, 0);
		if (ctx->data.stdinOpen)
			SetHandleInformation(ctx->data.stdinWrite, HANDLE_FLAG_INHERIT, 0);
	}


	// Create the child process
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);

	// Default any stream the caller did not request to this process's own standard handle, so a
	// child spawned with only (say) stdout piped still has a working stderr/stdin
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	if (ctx->data.stderrOpen)
		si.hStdError = ctx->data.stderrWrite;
	if (ctx->data.stdoutOpen)
		si.hStdOutput = ctx->data.stdoutWrite;
	if (ctx->data.stdinOpen)
		si.hStdInput = ctx->data.stdinRead;

	si.dwFlags |= STARTF_USESTDHANDLES;

	//// HOLY SHIT I CANNOT BELIEVE. Ok ok, so welcome to my TED talk, here I will talk about the confusion that is the Win32 C/C++ API and the absolute mental torment I went trough.
	//// Alright so HOW DID I MISS THE WHOLE OF STACK OVERFLOW WHEN TALKING ABOUT THIS. Ok so here's the issue, I started working on this library some time ago and I searched for a way
	//// to terminate a process. All of stack overflow said that I should use TerminateProcess and then close the handles, however for some reason this didn't close the process.
	//// Months after that I decided to shoot my shot again, didn't work. For hours on end, I was searching for the solution yet found nothing that works, tried opening the process with
	//// the PROCESS_TERMINATE flag, changed termination to the PID and even considered wrapping it in a job object. TURNS OUT ALL I NEEDED TO DO IS ADD THIS FLAG, YET MSDN AND STACKOVERFLOW
	//// HAD NOTHING TO SAY ABOUT IT. So... how did I found it. So I tested some stuff on ChatGPT and tried my luck, and I shit you not, the first answer gave me the solution
	constexpr DWORD creationFlags = CREATE_NEW_CONSOLE;
	if (!CreateProcess(filename, str.data(), nullptr, nullptr, true, creationFlags, nullptr, nullptr, &si, &ctx->data.pif))
	{
		// Every pipe handle created above would otherwise leak for the lifetime of the process.
		// init() has already flipped bValid/bCanUpdate on by this point, so a later destroy() would
		// also feed the still-zeroed PROCESS_INFORMATION to TerminateProcess and CloseHandle
		closePipeHandles(ctx->data);
		ctx->data.stderrOpen = false;
		ctx->data.stdoutOpen = false;
		ctx->data.stdinOpen = false;
		return -1;
	}

	// Close the ends the child owns so we can read from ours and it sees EOF on exit
	closeHandleEnd(ctx->data.stderrWrite);
	closeHandleEnd(ctx->data.stdoutWrite);
	closeHandleEnd(ctx->data.stdinRead);

	return 0;
}

void uexec::InternalWindows::updateWindows(ScriptRunner* ctx) noexcept
{
	if (ctx->data.pif.hProcess == nullptr)
		return;

	if (WaitForSingleObject(ctx->data.pif.hProcess, 0) != WAIT_TIMEOUT)
	{
		WaitForSingleObject(ctx->data.pif.hProcess, INFINITE);
		ctx->terminate();
	}
}

void uexec::InternalWindows::closeDescriptorsWindows(ScriptRunner* ctx) noexcept
{
	closePipeHandles(ctx->data);
}

void uexec::InternalWindows::destroyForReuseWindows(ScriptRunner* ctx) noexcept
{
	// Release everything the run left behind before the object is handed back for another init().
	// Resetting bFinished on its own left stale handle values in place for the next teardown to
	// close a second time, and closing a recycled handle takes out whatever now owns that value
	closePipeHandles(ctx->data);
	closeProcessHandles(ctx->data.pif);
	ctx->data.bFinished = false;
}

bool uexec::InternalWindows::finishedWindows(const ScriptRunner* const ctx) noexcept
{
	return ctx->data.bFinished;
}

void uexec::InternalWindows::terminateWindows(ScriptRunner* ctx) noexcept
{
	// Go to the essay in initWindows for the angry assay about CREATE_NEW_CONSOLE
	if (ctx->data.pif.hProcess != nullptr)
	{
		TerminateProcess(ctx->data.pif.hProcess, 0);
		// Termination is asynchronous - wait for the process to actually go before dropping the
		// handle, otherwise the exit is still in flight when the caller moves on
		WaitForSingleObject(ctx->data.pif.hProcess, INFINITE);
	}

	// All of these invalidate as they go, so terminate() followed by destroy() - the sequence the
	// public header tells callers to use - cannot double close
	closeProcessHandles(ctx->data.pif);
	closePipeHandles(ctx->data);

	ctx->data.bFinished = true;
}

bool uexec::InternalWindows::writeWindows(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	bytesWritten = 0;

	// Clamp to what the buffer actually holds, and honour the caller's size the way the Unix
	// backend does rather than ignoring it outright
	size_t maxWrite = size < buffer.size() ? size : buffer.size();
	if (maxWrite > MAXDWORD)
		maxWrite = MAXDWORD;
	if (ctx->data.stdinWrite == nullptr || maxWrite == 0)
		return false;

	DWORD writeBytes = 0;
	if (!WriteFile(ctx->data.stdinWrite, buffer.data(), static_cast<DWORD>(maxWrite), &writeBytes, nullptr))
		return false;

	bytesWritten = writeBytes;
	return bytesWritten > 0;
}

bool uexec::InternalWindows::readWindows(ScriptRunner* ctx, void* fd, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
	bytesRead = 0;

	size_t maxRead = size < buffer.size() ? size : buffer.size();
	if (maxRead > MAXDWORD)
		maxRead = MAXDWORD;
	if (fd == nullptr || maxRead == 0)
		return false;

	DWORD readBytes = 0;
	if (!ReadFile(fd, buffer.data(), static_cast<DWORD>(maxRead), &readBytes, nullptr))
		return false;

	bytesRead = readBytes;
	// A zero-byte read is end of stream, reported the same way the Unix backend reports it
	return bytesRead > 0;
}

#else
int uexec::InternalWindows::execandwaitWindows(char* const* command) noexcept
{
	return -1;
}

int uexec::InternalWindows::initWindows(char* const* command, ScriptRunner* ctx) noexcept
{
	return -1;
}

void uexec::InternalWindows::updateWindows(ScriptRunner* ctx) noexcept
{
}

void uexec::InternalWindows::closeDescriptorsWindows(ScriptRunner* ctx) noexcept
{
}

void uexec::InternalWindows::destroyForReuseWindows(ScriptRunner* ctx) noexcept
{
}

bool uexec::InternalWindows::finishedWindows(const ScriptRunner* const ctx) noexcept
{
	return false;
}

void uexec::InternalWindows::terminateWindows(ScriptRunner* ctx) noexcept
{
}

bool uexec::InternalWindows::writeWindows(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	return false;
}

bool uexec::InternalWindows::readWindows(ScriptRunner* ctx, void* fd, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
	return false;
}
#endif
