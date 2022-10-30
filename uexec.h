#pragma once
#ifdef UEXEC_CUSTOM_STRING
	#ifdef UEXEC_CUSTOM_STRING_INCLUDE
		#include UEXEC_CUSTOM_STRING_INCLUDE
		typedef UEXEC_CUSTOM_STRING uexecstring;
	#else
		#error UEXEC_CUSTOM_STRING defined but UEXEC_CUSTOM_STRING_INCLUDE not defined, it is needed to include the necessary headers for the string, and should contain the name of the header wrapped in ""
	#endif
#else
	#include <string>
	typedef std::string uexecstring;
#endif
#include <vector>
#ifdef _WIN32
	#include <windows.h>
	#include <thread>
#endif

namespace uexec
{
	// An abstraction over execvp and wait
	int execandwait(char* const* command) noexcept;
	
	class ScriptRunner
	{
	public:
		// Given an array, will start executing the script, will return -1 on failure
		int init(char* const* args, uint32_t size) noexcept;
		// Updates the buffer stream, call this with true the first time
		void update(bool bFirst = false) noexcept;
		// Will update the size of the buffer
		void updateBufferSize() noexcept;
		// Destroys the runner
		void destroy() noexcept;
		[[nodiscard]] bool valid() const noexcept;
		// Makes the runner reusable
		void destroyForReuse() noexcept;
		[[nodiscard]] bool finished() const noexcept;
        [[nodiscard]] bool startable() const noexcept;
		std::vector<uexecstring>& data() noexcept;

        // Terminates the process, use the destroy functions after calling terminate!
        void terminate() noexcept;
	private:
		std::vector<uexecstring> lineBuffer;
#ifdef _WIN32
		PHANDLE pipehandles[2];
		PROCESS_INFORMATION pif;
		std::thread thread;
		bool bFinished = false;
#else
		int pipefd[2]; // pipe file descriptors
		int pid = -1;
#endif
		uexecstring stringBuffer;
		bool bCanUpdate = false;
		bool bValid = true;
        bool bCanRestart = true;
		uint32_t bufferSize = 128;
	};

#ifndef _WIN32
	inline int currentpid = -1;
#endif
}