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


#define WIN_PIPE_READ 0
#define WIN_PIPE_WRITE 1


namespace uexec
{
	// An abstraction over execvp and wait
	int execandwait(char* const* command) noexcept;
	
	enum UExecStreams
	{
		UEXEC_STREAM_STD_OUT,
		UEXEC_STREAM_STD_IN,
		UEXEC_STREAM_STD_ERR,

	};

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

		// Read from STDOUT, STDIN and STDERR
		bool read(UExecStreams stream, std::string& buffer, size_t size, size_t& bytesRead) noexcept;

		// Write to STDOUT, STDIN and STDERR
		bool write(UExecStreams stream, std::string& buffer, size_t size, size_t& bytesWritten) noexcept;
	private:
		friend class InternalUnix;
		friend class InternalWindows;

		std::vector<uexecstring> lineBuffer;	// The buffer of lines
#ifdef _WIN32
		PROCESS_INFORMATION pif;				// The process information struct, contains a handle to the process
		HANDLE process;							// The PID, we use this to terminate the process
		
		// [0] READ, [1] WRITE, you can use the WIN_PIPE_READ and WIN_PIPE_WRITE macros for this
		HANDLE pipefd[2];
		SECURITY_ATTRIBUTES sa;

		bool bFinished = false;					// This indicates whether the process has finished executing in order to destroy it
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