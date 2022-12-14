#pragma once
#include <cinttypes>
#include "uexec.hpp"

namespace uexec
{
	class ScriptRunner;

	class InternalWindows
	{
	public:
		static int execandwaitWindows(char* const* command) noexcept;
	private:
		friend class ScriptRunner;
		
		static int initWindows(char* const* args, ScriptRunner* ctx) noexcept;
		static void updateWindows(bool bFirst, ScriptRunner* ctx) noexcept;
		static void destroyForReuseWindows(ScriptRunner* ctx) noexcept;
		static bool finishedWindows(const ScriptRunner* ctx) noexcept;
		static void terminateWindows(ScriptRunner* ctx) noexcept;

		static bool writeWindows(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept;
		static bool readWindows(ScriptRunner* ctx, void* fd, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept;
	};
}