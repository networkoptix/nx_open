#pragma once

#include <stdbool.h>
#include <stdio.h>

//-------------------------------------------------------------------------------------------------
// Configuration

// Enable/disable particular hooks
#define HOOK_OPEN
#define HOOK_CLOSE
#define HOOK_IOCTL

#define xCONF_ENABLE_OUTPUT
#define xCONF_SILENT
#define CONF_IOCTL_SPECIAL //< Special handling of ioctl() to /dev/disp and /dev/fb0.

struct Conf
{
    bool logStructs;
    bool serializeFbAndDisp;
};
#define CONF_DEFINITION \
__attribute__ ((visibility ("hidden"))) const struct Conf conf = \
{ \
    .logStructs = false, \
    .serializeFbAndDisp = true, \
};
extern __attribute__ ((visibility ("hidden"))) const struct Conf conf;

//-------------------------------------------------------------------------------------------------

#if defined(CONF_ENABLE_OUTPUT)
    #define OUTPUT(format, args...) fprintf(stderr, "[HOOK] " format "\n", ## args)
#else
    #define OUTPUT(...) do {} while (0)
#endif // CONF_MAIN_LOG

#if !defined(CONF_SILENT)
    #define PRINT(format, args...) fprintf(stderr, "[HOOK] " format "\n", ## args)
#else
    #define PRINT(...) do {} while (0)
#endif // CONF_FULL_LOG

typedef unsigned long request_t;

typedef int (*IoctlFunc)(int, request_t, void*);

