#pragma once

#ifdef UVK_LOG_EXPORT_FROM_LIBRARY
    #ifdef _WIN32
        #ifdef UVK_LIB_COMPILE
            #define UVK_PUBLIC_API __declspec(dllexport)
            #define UVK_PUBLIC_TMPL_API __declspec(dllexport)
        #else
            #define UVK_PUBLIC_API __declspec(dllimport)
            #define UVK_PUBLIC_TMPL_API
        #endif
    #else
        #define UVK_PUBLIC_API
        #define UVK_PUBLIC_TMPL_API
    #endif
#else
    #define UVK_PUBLIC_API
    #define UVK_PUBLIC_TMPL_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    #ifdef _WIN32
        #include <windows.h>
    #endif
    #include <stdbool.h>

    typedef struct UVK_PUBLIC_API uexec_RunnerData
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
        size_t pidpos;
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
