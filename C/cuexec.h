#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdlib.h>
#include "../uexecdata.h"

    int uexec_execandwait(char* const* command);

    // Given an array, will start executing the script, will return -1 on failure
    int uexec_runner_init(struct RunnerData* data, char* const* args, bool bOpenStderrPipe, bool bOpenStdoutPipe, bool bOpenStdinPipe);

    // Updates the buffer stream, call this with true the first time
    void uexec_runner_update(struct RunnerData* data);

    // Destroys the runner
    void uexec_runner_destroy(struct RunnerData* data);
    bool uexec_runner_valid(struct RunnerData* data);

    // Makes the runner reusable
    void uexec_runner_destroyForReuse(struct RunnerData* data);
    bool uexec_runner_finished(struct RunnerData* data);
    bool uexec_runner_startable(struct RunnerData* data);

    // Terminates the process, use the destroy functions after calling terminate!
    void uexec_runner_terminate(struct RunnerData* data);

    // Read from STDOUT, STDIN and STDERR
    bool uexec_runner_readSTDOUT(struct RunnerData* data, char* buffer, size_t size, size_t* bytesRead);
    bool uexec_runner_readSTDERR(struct RunnerData* data, char* buffer, size_t size, size_t* bytesRead);

    // Write to STDOUT, STDIN and STDERR
    bool uexec_runner_write(struct RunnerData* data, const char* buffer, size_t size, size_t* bytesWritten);
#ifdef __cplusplus
}
#endif