#pragma once

#if defined (TEST_PLUGIN_LOG)

    #if defined (__linux__) 
        #include <sys/time.h>
        #include <stdio.h>

        #define LOG(...) \
            do { \
                char ___buf[1024]; \
                struct timeval ___tval; \
                gettimeofday(&___tval, NULL); \
                strftime(___buf, sizeof(___buf), "%H:%M:%S", localtime(&___tval.tv_sec)); \
                sprintf(___buf + strlen(___buf), ".%06ld\t", ___tval.tv_usec); \
                sprintf(___buf + strlen(___buf), "%ld\t", pthread_self()); \
                sprintf(___buf + strlen(___buf), __VA_ARGS__); \
                fprintf(stdout, "%s\n", ___buf); \
                fflush(stdout); \
            } while (0)
    #endif

#else
    #define LOG(...)
#endif