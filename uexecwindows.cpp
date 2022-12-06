#include "uexecwindows.hpp"
#include "uexec.hpp"

#ifdef _WIN32
#include <windows.h>

int uexec::InternalWindows::execandwaitWindows(char* const* command) noexcept
{
	uexecstring ext = command[0];
	
	char filename[MAX_PATH];
	LPSTR filepart;
	
	if (!ext.empty() && !SearchPathA(nullptr, command[0], ext.substr(ext.find_last_of("."), ext.size() - 1).data(), MAX_PATH, filename, &filepart))
	{
		return -1;
	}
	
	uexecstring str = uexecstring(filename) + " ";
	for (size_t i = 1; command[i] != nullptr; i++)
		str += uexecstring(command[i]) + " ";
	
	PROCESS_INFORMATION pif;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pif, sizeof(pif));
	si.cb = sizeof(si);
	
	DWORD creationFlags = CREATE_NEW_CONSOLE;
	if (!CreateProcess(filename, str.data(), nullptr, nullptr, false, creationFlags, nullptr, nullptr, &si, &pif));
	{
		return -1;
	}
	
	WaitForSingleObject(pif.hProcess, INFINITE);
	WaitForSingleObject(pif.hThread, INFINITE);
	CloseHandle(pif.hProcess);
	CloseHandle(pif.hThread);
	return 0;
}

int uexec::InternalWindows::initWindows(char* const* args, uint32_t size, ScriptRunner* ctx) noexcept
{
	//bool bSuccess = CreatePipe(pipehandles[0], pipehandles[2], nullptr, 0);
	uexecstring ext = args[0];
	char filename[MAX_PATH];
	LPSTR filepart;
	
	if (!ext.empty() && !SearchPathA(nullptr, args[0], ext.substr(ext.find_last_of("."), ext.size() - 1).data(), MAX_PATH, filename, &filepart))
	{
		return -1;
	}
	
	uexecstring str = uexecstring(filename) + " ";
	for (size_t i = 1; args[i] != nullptr; i++)
		str += uexecstring(args[i]) + " ";
	
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&ctx->pif, sizeof(ctx->pif));
	ZeroMemory(&ctx->process, sizeof(ctx->process));

	// HOLY SHIT I CANNOT BELIEVE. Ok ok, so welcome to my TED talk, here I will talk about the confusion that is the Win32 C/C++ API and the absolute mental torment I went trough.
	// Alright so HOW DID I MISS THE WHOLE OF STACK OVERFLOW WHEN TALKING ABOUT THIS. Ok so here's the issue, I started working on this library some time ago and I searched for a way
	// to terminate a process. All of stack overflow said that I should use TerminateProcess and then close the handles, however for some reason this didn't close the process.
	// Months after that I decided to shoot my shot again, didn't work. For hours on end, I was searching for the solution yet found nothing that works, tried opening the process with
	// the PROCESS_TERMINATE flag, changed termination to the PID and even considered wrapping it in a job object. TURNS OUT ALL I NEEDED TO DO IS ADD THIS FLAG, YET MSDN AND STACKOVERFLOW
	// HAD NOTHING TO SAY ABOUT IT. So... how did I found it. So I tested some stuff on ChatGPT and tried my luck, and I shit you not, the first answer gave me the solution
	DWORD creationFlags = CREATE_NEW_CONSOLE;
	si.cb = sizeof(si);
	if (!CreateProcess(filename, str.data(), nullptr, nullptr, false, creationFlags, nullptr, nullptr, &si, &ctx->pif));
	{
		return -1;
	}
}

void uexec::InternalWindows::updateWindows(bool bFirst, ScriptRunner* ctx) noexcept
{
	if (WaitForSingleObject(ctx->pif.hProcess, 0) != WAIT_TIMEOUT)
	{
		WaitForSingleObject(ctx->pif.hProcess, INFINITE);
		ctx->terminate();
	}
}

void uexec::InternalWindows::destroyForReuseWindows(ScriptRunner* ctx) noexcept
{
	ctx->bFinished = false;
	ctx->pipehandles[0] = nullptr;
	ctx->pipehandles[1] = nullptr;
	ctx->process = 0;
}

bool uexec::InternalWindows::finishedWindows(const ScriptRunner* const ctx) noexcept
{
	return false;
}

void uexec::InternalWindows::terminateWindows(ScriptRunner* ctx) noexcept
{
	// Go to line 69 for angry assay
	TerminateProcess(ctx->pif.hProcess, 0);
	CloseHandle(ctx->pif.hThread);
	CloseHandle(ctx->pif.hProcess);
	
	ctx->bFinished = true;
}

#else
int uexec::InternalWindows::execandwaitUnix(char* const* command) noexcept
{
	return -1;
}

int uexec::InternalWindows::initWindows(char* const* command, uint32_t size, ScriptRunner* ctx) noexcept
{
	return -1;
}

void uexec::InternalWindows::updateWindows(bool bFirst, ScriptRunner* ctx) noexcept
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
#endif