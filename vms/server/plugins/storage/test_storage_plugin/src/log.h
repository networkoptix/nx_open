// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#define TEST_PLUGIN_LOG

#if defined(TEST_PLUGIN_LOG)

    #if defined(__linux__)
        #include <pthread.h>
        #include <stdio.h>

        #include <sys/time.h>

        #define LOG(...) \
            do \
            { \
                char ___buf[4096]; \
                struct timeval ___tval; \
                gettimeofday(&___tval, NULL); \
                strftime(___buf, sizeof(___buf), "%H:%M:%S", localtime(&___tval.tv_sec)); \
                sprintf(___buf + strlen(___buf), ".%06ld\t", ___tval.tv_usec); \
                sprintf(___buf + strlen(___buf), "%ld\t", pthread_self()); \
                snprintf(___buf + strlen(___buf), 4096 - strlen(___buf), __VA_ARGS__); \
                fprintf(stdout, "%s\n", ___buf); \
                fflush(stdout); \
            } while (0)
    #elif defined (_WIN32) || defined(__APPLE__)
        #define LOG(...)
    #endif

#else
    #define LOG(...)
#endif
