#pragma once
#include <cinttypes>
#include <string>
#include "uexec.hpp"

namespace uexec
{
	class ScriptRunner;

	class InternalUnix
	{
	public:
		static int execandwaitunix(char* const* command) noexcept;
	private:
		friend class ScriptRunner;

		static int initUnix(char* const* args, ScriptRunner* ctx) noexcept;
		static void destroyForReuseUnix(ScriptRunner* ctx) noexcept;
		static bool finishedUnix(const ScriptRunner* ctx) noexcept;
		static void terminateUnix(ScriptRunner* ctx) noexcept;

		static bool readUnix(ScriptRunner* ctx, int* pipe, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept;
		static bool writeUnix(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept;
	};
}