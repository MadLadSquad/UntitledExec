#pragma once
#ifdef _WIN32
    #include <windows.h>
#endif

extern "C"
{
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
        int pid;
    #endif
        bool stderrOpen;
        bool stdoutOpen;
        bool stdinOpen;

        bool bCanUpdate;
        bool bValid;
        bool bCanRestart;
    };
}