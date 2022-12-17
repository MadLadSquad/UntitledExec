#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    #ifdef _WIN32
        #include <windows.h>
    #endif


    struct RunnerData
    {
    #ifdef _WIN32
        PROCESS_INFORMATION pif;				// The process information struct, contains a handle to the process
        HANDLE stderrRead, stdoutRead, stdinRead;
        HANDLE stderrWrite, stdoutWrite, stdinWrite;

        bool bFinished;					// This indicates whether the process has finished executing in order to destroy it
    #else
        int pipefdSTDIN[2];
        int pipefdSTDOUT[2];
        int pipefdSTDERR[2];

        pid_t* pidp;
        size_t pidpos;
    #endif
        bool stderrOpen;
        bool stdoutOpen;
        bool stdinOpen;

        bool bCanUpdate;
        bool bValid;
        bool bCanRestart;
    };
#ifdef __cplusplus
}
#endif