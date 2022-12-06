#pragma once
#include <cinttypes>

namespace uexec
{
	class ScriptRunner;

	class InternalWindows
	{
	public:
		static int execandwaitWindows(char* const* command) noexcept;
	private:
		friend class ScriptRunner;
		
		static int initWindows(char* const* args, uint32_t size, ScriptRunner* ctx) noexcept;
		static void updateWindows(bool bFirst, ScriptRunner* ctx) noexcept;
		static void destroyForReuseWindows(ScriptRunner* ctx) noexcept;
		static bool finishedWindows(const ScriptRunner* const ctx) noexcept;
		static void terminateWindows(ScriptRunner* ctx) noexcept;
	};
}