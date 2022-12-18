#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    #include <stdlib.h>
    #include "../uexecdata.h"

    UVK_PUBLIC_API int uexec_execandwait(char* const* command);

    // Given an array, will start executing the script, will return -1 on failure
    UVK_PUBLIC_API int uexec_runner_init(struct RunnerData* data, char* const* args, bool bOpenStderrPipe, bool bOpenStdoutPipe, bool bOpenStdinPipe);

    // Updates the buffer stream, call this with true the first time
    UVK_PUBLIC_API void uexec_runner_update(struct RunnerData* data);

    // Destroys the runner
    UVK_PUBLIC_API void uexec_runner_destroy(struct RunnerData* data);
    UVK_PUBLIC_API bool uexec_runner_valid(struct RunnerData* data);

    // Makes the runner reusable
    UVK_PUBLIC_API void uexec_runner_destroyForReuse(struct RunnerData* data);
    UVK_PUBLIC_API bool uexec_runner_finished(struct RunnerData* data);
    UVK_PUBLIC_API bool uexec_runner_startable(struct RunnerData* data);

    // Terminates the process, use the destroy functions after calling terminate!
    UVK_PUBLIC_API void uexec_runner_terminate(struct RunnerData* data);

    // Read from STDOUT, STDIN and STDERR
    UVK_PUBLIC_API bool uexec_runner_readSTDOUT(struct RunnerData* data, char* buffer, size_t size, size_t* bytesRead);
    UVK_PUBLIC_API bool uexec_runner_readSTDERR(struct RunnerData* data, char* buffer, size_t size, size_t* bytesRead);

    // Write to STDOUT, STDIN and STDERR
    UVK_PUBLIC_API bool uexec_runner_write(struct RunnerData* data, const char* buffer, size_t size, size_t* bytesWritten);
#ifdef __cplusplus
}
#endif