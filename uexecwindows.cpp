#include "uexecwindows.hpp"
#include "uexec.hpp"

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <string>

namespace
{
	// Appends a single argument to a command line, quoting/escaping it per the rules used by
	// CommandLineToArgvW / the MSVC CRT so that arguments containing spaces, tabs or quotes survive
	// intact. Without this, any argument with a space would be split by the child process.
	void appendQuotedArg(std::string& out, const char* arg) noexcept
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
	uexecstring buildCommandLine(const char* filename, char* const* args) noexcept
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
}

int uexec::InternalWindows::execandwaitWindows(char* const* command) noexcept
{
	uexecstring ext = command[0];
	
	char filename[MAX_PATH];
	LPSTR filepart;
	
	if (!ext.empty() && !SearchPathA(nullptr, command[0], ext.substr(ext.find_last_of("."), ext.size() - 1).data(), MAX_PATH, filename, &filepart))
	{
		return -1;
	}
	
	uexecstring str = buildCommandLine(filename, command);

	PROCESS_INFORMATION pif;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pif, sizeof(pif));
	si.cb = sizeof(si);

	DWORD creationFlags = CREATE_NEW_CONSOLE;
	if (!CreateProcess(filename, str.data(), nullptr, nullptr, false, creationFlags, nullptr, nullptr, &si, &pif))
	{
		return -1;
	}
	
	WaitForSingleObject(pif.hProcess, INFINITE);
	WaitForSingleObject(pif.hThread, INFINITE);
	CloseHandle(pif.hProcess);
	CloseHandle(pif.hThread);
	return 0;
}

int uexec::InternalWindows::initWindows(char* const* args, ScriptRunner* ctx) noexcept
{
	//bool bSuccess = CreatePipe(pipehandles[0], pipehandles[2], nullptr, 0);
	uexecstring ext = args[0];
	char filename[MAX_PATH];
	LPSTR filepart;
	
	if (!ext.empty() && !SearchPathA(nullptr, args[0], ext.substr(ext.find_last_of("."), ext.size() - 1).data(), MAX_PATH, filename, &filepart))
	{
		return -1;
	}
	
	uexecstring str = buildCommandLine(filename, args);

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
	ZeroMemory(&ctx->data.pif, sizeof(PROCESS_INFORMATION));

	//// HOLY SHIT I CANNOT BELIEVE. Ok ok, so welcome to my TED talk, here I will talk about the confusion that is the Win32 C/C++ API and the absolute mental torment I went trough.
	//// Alright so HOW DID I MISS THE WHOLE OF STACK OVERFLOW WHEN TALKING ABOUT THIS. Ok so here's the issue, I started working on this library some time ago and I searched for a way
	//// to terminate a process. All of stack overflow said that I should use TerminateProcess and then close the handles, however for some reason this didn't close the process.
	//// Months after that I decided to shoot my shot again, didn't work. For hours on end, I was searching for the solution yet found nothing that works, tried opening the process with
	//// the PROCESS_TERMINATE flag, changed termination to the PID and even considered wrapping it in a job object. TURNS OUT ALL I NEEDED TO DO IS ADD THIS FLAG, YET MSDN AND STACKOVERFLOW
	//// HAD NOTHING TO SAY ABOUT IT. So... how did I found it. So I tested some stuff on ChatGPT and tried my luck, and I shit you not, the first answer gave me the solution
	constexpr DWORD creationFlags = CREATE_NEW_CONSOLE;
	if (!CreateProcess(filename, str.data(), nullptr, nullptr, true, creationFlags, nullptr, nullptr, &si, &ctx->data.pif))
		return -1;

	// Close the write ends of the pipes so we can read from them
	if (ctx->data.stderrOpen)
		CloseHandle(ctx->data.stderrWrite);
	if (ctx->data.stdoutOpen)
		CloseHandle(ctx->data.stdoutWrite);
	if (ctx->data.stdinOpen)
		CloseHandle(ctx->data.stdinRead);

	return 0;
}

void uexec::InternalWindows::updateWindows(ScriptRunner* ctx) noexcept
{
	if (WaitForSingleObject(ctx->data.pif.hProcess, 0) != WAIT_TIMEOUT)
	{
		WaitForSingleObject(ctx->data.pif.hProcess, INFINITE);
		ctx->terminate();
	}
}

void uexec::InternalWindows::destroyForReuseWindows(ScriptRunner* ctx) noexcept
{
	ctx->data.bFinished = false;
}

bool uexec::InternalWindows::finishedWindows(const ScriptRunner* const ctx) noexcept
{
	return false;
}

void uexec::InternalWindows::terminateWindows(ScriptRunner* ctx) noexcept
{
	// Go to line 94 for angry assay
	TerminateProcess(ctx->data.pif.hProcess, 0);

	CloseHandle(ctx->data.pif.hThread);
	CloseHandle(ctx->data.pif.hProcess);

	if (ctx->data.stderrOpen)
		CloseHandle(ctx->data.stderrRead);
	if (ctx->data.stdoutOpen)
		CloseHandle(ctx->data.stdoutRead);
	if (ctx->data.stdinOpen)
		CloseHandle(ctx->data.stdinWrite);

	ctx->data.bFinished = true;
}

bool uexec::InternalWindows::writeWindows(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept
{
	DWORD writeBytes;

	auto result = WriteFile(ctx->data.stdinWrite, buffer.data(), buffer.size(), &writeBytes, nullptr);
	bytesWritten = writeBytes;

	return result;
}

bool uexec::InternalWindows::readWindows(ScriptRunner* ctx, void* fd, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept
{
	DWORD readBytes;
	
	auto result = ReadFile(fd, buffer.data(), buffer.size(), &readBytes, nullptr);
	bytesRead = readBytes;
	return result;
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
