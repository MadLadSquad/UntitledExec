#pragma once

// This is an installed public header, so it has to be self-contained rather than relying on the
// consumer having included these first - pid_t used to resolve only by accident, through a
// transitive include from uexec.hpp or from glibc's <stdlib.h> under _DEFAULT_SOURCE. Included
// outside the extern "C" block below, since a system header may contain C++ constructs of its own
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/types.h>
#endif
#include <stdbool.h>

#ifdef MLS_EXPORT_LIBRARY
    #ifdef _WIN32
        #ifdef MLS_LIB_COMPILE
            #define MLS_PUBLIC_API __declspec(dllexport)
        #else
            #define MLS_PUBLIC_API __declspec(dllimport)
        #endif
    #else
        #define MLS_PUBLIC_API
    #endif
#else
    #define MLS_PUBLIC_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct MLS_PUBLIC_API uexec_RunnerData
    {
    #ifdef _WIN32
        PROCESS_INFORMATION pif;                        // The process information struct, contains a handle to the process
        HANDLE stderrRead, stdoutRead, stdinRead;
        HANDLE stderrWrite, stdoutWrite, stdinWrite;

        bool bFinished;                                 // This indicates whether the process has finished executing in order to destroy it
    #else
        int pipefdSTDIN[2];
        int pipefdSTDOUT[2];
        int pipefdSTDERR[2];

        pid_t* pidp;
    #endif
        bool stderrOpen;
        bool stdoutOpen;
        bool stdinOpen;

        bool bCanUpdate;
        bool bValid;
        bool bCanRestart;
    } uexec_RunnerData;
#ifdef __cplusplus
};
#endif
