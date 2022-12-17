struct RunnerData;

#include "cuexec.h"
#include "../uexec.hpp"
#include <memory.h>

#define cast(x) reinterpret_cast<uexec::ScriptRunner*>(x)

extern "C"
{
    int uexec_execandwait(char* const* command)
    {
        return uexec::execandwait(command);
    }

    int uexec_runner_init(struct RunnerData* data, char* const* args, bool bOpenStderrPipe, bool bOpenStdoutPipe, bool bOpenStdinPipe)
    {
        return cast(data)->init(args, bOpenStderrPipe, bOpenStdoutPipe, bOpenStdinPipe);
    }

    void uexec_runner_update(struct RunnerData* data)
    {
        cast(data)->update();
    }

    void uexec_runner_destroy(struct RunnerData* data)
    {
        cast(data)->destroy();
    }

    bool uexec_runner_valid(struct RunnerData* data)
    {
        return cast(data)->valid();
    }

    void uexec_runner_destroyForReuse(struct RunnerData* data)
    {
        cast(data)->destroyForReuse();
    }

    bool uexec_runner_finished(struct RunnerData* data)
    {
        return cast(data)->finished();
    }

    bool uexec_runner_startable(struct RunnerData* data)
    {
        return cast(data)->startable();
    }

    void uexec_runner_terminate(struct RunnerData* data)
    {
        cast(data)->terminate();
    }

    bool uexec_runner_readSTDOUT(struct RunnerData* data, char* buffer, size_t size, size_t* bytesRead)
    {
        uexecstring str;
        str.resize(size);
        auto result = cast(data)->readSTDOUT(str, size, *bytesRead);

        size_t cpysize = *bytesRead > size ? size : *bytesRead;
        str.erase(cpysize);
        if (!str.empty())
            memcpy(buffer, str.data(), cpysize);
        return result;
    }

    bool uexec_runner_readSTDERR(struct RunnerData* data, char* buffer, size_t size, size_t* bytesRead)
    {
        uexecstring str;
        str.resize(size);
        auto result = cast(data)->readSTDERR(str, size, *bytesRead);

        size_t cpysize = *bytesRead > size ? size : *bytesRead;
        str.erase(cpysize);
        if (!str.empty())
            memcpy(buffer, str.data(), cpysize);
        return result;
    }

    bool uexec_runner_write(struct RunnerData* data, const char* buffer, size_t size, size_t* bytesWritten)
    {
        uexecstring str = buffer;
        return cast(data)->write(str, size, *bytesWritten);
    }
}