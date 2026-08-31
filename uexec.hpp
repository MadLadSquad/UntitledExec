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
#include <list>
#ifdef _WIN32
	#include <windows.h>
	#include <thread>
#endif

#include "uexecdata.h"

namespace uexec
{
	// An abstraction over execvp and wait
	// UntitledImGuiFramework Event Safety - Any time
	MLS_PUBLIC_API int execandwait(char* const* command) noexcept;

	// UntitledImGuiFramework Event Safety - Any time
	class MLS_PUBLIC_API ScriptRunner
	{
	public:
		// Given an array, will start executing the script, will return -1 on failure
		int init(char* const* args, bool bOpenStderrPipe = false, bool bOpenStdoutPipe = false, bool bOpenStdinPipe = false) noexcept;
		// Updates the buffer stream, call this with true the first time
		void update() noexcept;
		// Destroys the runner
		void destroy() noexcept;
		[[nodiscard]] bool valid() const noexcept;
		// Makes the runner reusable
		void destroyForReuse() noexcept;
		[[nodiscard]] bool finished() const noexcept;
        [[nodiscard]] bool startable() const noexcept;

        // Terminates the process, use the destroy functions after calling terminate!
        void terminate() noexcept;

		// Read from STDOUT, STDIN and STDERR
		bool readSTDOUT(uexecstring& buffer, size_t size, size_t& bytesRead) noexcept;
		bool readSTDERR(uexecstring& buffer, size_t size, size_t& bytesRead) noexcept;

		// Write to STDOUT, STDIN and STDERR
		bool write(uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept;
	private:
		friend class InternalUnix;
		friend class InternalWindows;

		// Value-initialised so that valid()/finished()/startable()/update() never read
		// indeterminate memory when called before init()
		uexec_RunnerData data{};
	};

#ifndef _WIN32
	// A std::list is used instead of a std::vector so that the address of each runner's slot
	// (stored in RunnerData::pidp) stays valid regardless of insertions/erasures of other
	// runners - a vector would invalidate those pointers on reallocation and shift indices on erase
	inline std::list<pid_t> uexec_internal_pids_do_not_touch;
#endif
}
