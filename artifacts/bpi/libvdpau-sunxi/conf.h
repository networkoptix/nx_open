#pragma once

#define CONFIG_MODULE_NAME "vdpau_sunxi"
#define CONFIG_BODY \
{ \
    NX_FLAG(0, enableOutput, "Log all calls to VDPAU interface functions."); \
    NX_FLAG(0, enableColorKey, "Enable disp call DISP_CMD_SET_COLORKEY..."); \
    NX_FLAG(0, enableIoctlOutput, "Log all disp calls: ioctl(/dev/disp, ...)."); \
    NX_FLAG(0, enableSetFb, "Use disp call DISP_CMD_LAYER_SET_FB instead of DISP_CMD_SET_PARA and DISP_CMD_LAYER_OPEN."); \
    NX_FLAG(0, enableXClearWindow, "Enable call to XClearWindow (was originally enabled)."); \
    NX_FLAG(0, disableCscMatrix, "Disable disp calls for CSC Matrix params which also turn on Enhancement."); \
}

#include "flag_config_c.h"
