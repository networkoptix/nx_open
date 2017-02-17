#pragma once

#if defined (__linux__) && defined (TEST_PLUGIN_LOG)
    #include <sys/time.h>
    #include <stdio.h>

    #define LOG(...) \
        do { \
            char buf[1024]; \
            struct timeval tval; \
            gettimeofday(&tval, NULL); \
            strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&tval.tv_sec)); \
            sprintf(buf + strlen(buf), ".%06ld\t", tval.tv_usec); \
            sprintf(buf + strlen(buf), "%ld\t", pthread_self()); \
            sprintf(buf + strlen(buf), __VA_ARGS__); \
            fprintf(stdout, "%s\n", buf); \
            fflush(stdout); \
        } while (0)

#else
    #define LOG(...)
#endif