#pragma once
#include <cinttypes>
#include <string>
#include "uexec.hpp"

namespace uexec
{
	class ScriptRunner;
	enum UExecStreams;

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

		static bool readUnix(ScriptRunner* ctx, UExecStreams stream, uexecstring& buffer, size_t size, size_t& bytesRead) noexcept;
		static bool writeUnix(ScriptRunner* ctx, uexecstring& buffer, size_t size, size_t& bytesWritten) noexcept;
	};
}