#include "uexec.h"
#ifdef _WIN32
	#include <windows.h>
	#include <iostream>
#else
	#include <unistd.h>
	#include <sys/wait.h>
#endif
#include <csignal>

int uexec::execandwait(char* const* command)
{
#ifdef _WIN32
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
	si.cb = sizeof(si);

	if (!CreateProcessA(filename, str.data(), nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pif));
	{
		return -1;
	}

	WaitForSingleObject(pif.hProcess, INFINITE);
	WaitForSingleObject(pif.hThread, INFINITE);
	CloseHandle(pif.hProcess);
	CloseHandle(pif.hThread);
#else
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
#endif
}

int uexec::ScriptRunner::init(char* const* args, uint32_t size)
{
	stringBuffer.reserve(size);
	bufferSize = size;

#ifdef _WIN32

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

	PROCESS_INFORMATION pif;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	if (!CreateProcessA(filename, str.data(), nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pif));
	{
		return -1;
	}

	thread = std::thread([&]() {
		WaitForSingleObject(pif.hProcess, INFINITE);
		WaitForSingleObject(pif.hThread, INFINITE);
		CloseHandle(pif.hProcess);
		CloseHandle(pif.hThread);
		bFinished = true;
	});
#else
	pid = fork();
	if (pid != -1 && pout != -1)
	{
		if (pid == 0)
		{
			close(pipefd[0]);

			dup2(pipefd[1], STDOUT_FILENO);
			dup2(pipefd[1], STDERR_FILENO);

			close(pipefd[1]);

			execvp(cmd[0], cmd);
		}
		else
		{
			bCanUpdate = true;
			currentpid = pid;
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
#endif
	return 0;
}

void uexec::ScriptRunner::update(bool bFirst)
{
	if (bCanUpdate)
	{
		
		
#ifdef _WIN32

#else
		if (pid > 0)
		{
			std::vector<char> buf;
			buf.reserve(bufferSize);
			if (bFirst)
				close(pipefd[1]);
			if (read(pipefd[0], buf.data(), sizeof(buf.data())) != 0)
			{
				uexecstring tmp = buf.data();
				tmp.shrink_to_fit();
				stringBuffer += tmp;
				uexecstring accumulate;
				for (auto& a : stringBuffer)
				{
					if (a == '\n')
					{
						lineBuffer.push_back(accumulate);
						accumulate.clear();
					}
					else
						accumulate += a;
				}
			}
		}
#endif
	}
}

void uexec::ScriptRunner::updateBufferSize()
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

void uexec::ScriptRunner::destroy()
{
	if (finished())
	{
#ifdef _WIN32
		if (thread.joinable())
			thread.join();
#endif
		bValid = false;
	}
}

bool uexec::ScriptRunner::valid() const
{
	return bValid;
}

void uexec::ScriptRunner::destroyForReuse()
{
	if (finished())
	{
		stringBuffer.clear();
		lineBuffer.clear();
		bCanUpdate = false;
		bValid = true;
		bFinished = false;
#ifdef _WIN32
		pipehandles[0] = nullptr;
		pipehandles[1] = nullptr;
		if (thread.joinable())
			thread.join();
#else
		pid = -1;
		pipefd[0] = -1;
		pipefd[1] = -1;
#endif
	}
}

bool uexec::ScriptRunner::finished() const
{
#ifdef _WIN32
	return bFinished;
#else
	return (bCanUpdate && currentpid == -1 ? true : false);
#endif
}

std::vector<uexecstring>& uexec::ScriptRunner::data()
{
	return lineBuffer;
}
