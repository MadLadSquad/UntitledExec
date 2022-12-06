#pragma once
#include <cinttypes>

namespace uexec
{
	class ScriptRunner;
	class InternalUnix
	{
	public:
		static int execandwaitunix(char* const* command) noexcept;
	private:
		friend class ScriptRunner;

		static int initUnix(char* const* args, uint32_t size, ScriptRunner* ctx) noexcept;
		static void updateUnix(bool bFirst, ScriptRunner* ctx) noexcept;
		static void destroyForReuseUnix(ScriptRunner* ctx) noexcept;
		static bool finishedUnix(const ScriptRunner* const ctx) noexcept;
		static void terminateUnix(ScriptRunner* ctx) noexcept;
	};
}