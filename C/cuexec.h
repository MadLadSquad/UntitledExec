#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    #include <stdlib.h>
    #include "../uexecdata.h"

    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API int uexec_execandwait(char* const* command);

    // Given an array, will start executing the script, will return -1 on failure
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API int uexec_runner_init(uexec_RunnerData* data, char* const* args, bool bOpenStderrPipe, bool bOpenStdoutPipe, bool bOpenStdinPipe);

    // Updates the buffer stream, call this with true the first time
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API void uexec_runner_update(uexec_RunnerData* data);

    // Destroys the runner
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API void uexec_runner_destroy(uexec_RunnerData* data);
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API bool uexec_runner_valid(uexec_RunnerData* data);

    // Makes the runner reusable
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API void uexec_runner_destroyForReuse(uexec_RunnerData* data);
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API bool uexec_runner_finished(uexec_RunnerData* data);
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API bool uexec_runner_startable(uexec_RunnerData* data);

    // Terminates the process, use the destroy functions after calling terminate!
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API void uexec_runner_terminate(uexec_RunnerData* data);

    // Read from STDOUT, STDIN and STDERR
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API bool uexec_runner_readSTDOUT(uexec_RunnerData* data, char* buffer, size_t size, size_t* bytesRead);
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API bool uexec_runner_readSTDERR(uexec_RunnerData* data, char* buffer, size_t size, size_t* bytesRead);

    // Write to STDOUT, STDIN and STDERR
    // UntitledImGuiFramework Event Safety - Any time
    MLS_PUBLIC_API bool uexec_runner_write(uexec_RunnerData* data, const char* buffer, size_t size, size_t* bytesWritten);
#ifdef __cplusplus
}
#endif
