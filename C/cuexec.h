#pragma once
#include <stdlib.h>
#include "../uexecdata.h"

extern "C"
{
    int uexec_execandwait(char* const* command);

    // Given an array, will start executing the script, will return -1 on failure
    int uexec_runner_init(RunnerData* data, char* const* args, bool bOpenStderrPipe, bool bOpenStdoutPipe, bool bOpenStdinPipe);

    // Updates the buffer stream, call this with true the first time
    void uexec_runner_update(RunnerData* data);

    // Destroys the runner
    void uexec_runner_destroy(RunnerData* data);
    bool uexec_runner_valid(RunnerData* data);

    // Makes the runner reusable
    void uexec_runner_destroyForReuse(RunnerData* data);
    bool uexec_runner_finished(RunnerData* data);
    bool uexec_runner_startable(RunnerData* data);

    // Terminates the process, use the destroy functions after calling terminate!
    void uexec_runner_terminate(RunnerData* data);

    // Read from STDOUT, STDIN and STDERR
    bool uexec_runner_readSTDOUT(RunnerData* data, char* buffer, size_t size, size_t* bytesRead);
    bool uexec_runner_readSTDERR(RunnerData* data, char* buffer, size_t size, size_t* bytesRead);

    // Write to STDOUT, STDIN and STDERR
    bool uexec_runner_write(RunnerData* data, const char* buffer, size_t size, size_t* bytesWritten);
}
