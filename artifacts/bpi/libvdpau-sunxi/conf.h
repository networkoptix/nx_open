#pragma once

#define CONFIG_MODULE_NAME "vdpau_sunxi"
#define CONFIG_BODY \
{ \
    NX_FLAG(1, enableOutput, ""); \
    NX_FLAG(0, enableColorKey, ""); \
    NX_FLAG(1, enableIoctlOutput, ""); \
    NX_FLAG(0, enableSetFb, ""); \
    NX_FLAG(0, enableXClearWindow, ""); \
    NX_FLAG(0, disableCscMatrix, ""); \
}

#include "flag_config_c.h"
