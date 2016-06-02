#pragma once

#define CONFIG_MODULE_NAME "vdpau_sunxi"
#define CONFIG_BODY \
{ \
    NX_FLAG(0, enableOutput, "Log all calls to VDPAU interface functions."); \
    NX_FLAG(0, enableColorKey, "Enable disp call DISP_CMD_SET_COLORKEY..."); \
    NX_FLAG(0, enableIoctlOutput, "Log all disp calls: ioctl(/dev/disp, ...)."); \
    NX_FLAG(0, enableSetFb, "Use disp call ...LAYER_SET_FB instead of ...SET_PARA and ...LAYER_OPEN."); \
    NX_FLAG(0, enableXClearWindow, "Enable call to XClearWindow (was originally enabled)."); \
    NX_FLAG(0, disableCscMatrix, "Disable disp calls for CSC Matrix params which also turn on Enhancement."); \
    NX_FLAG(0, enhancementOn, "Always issue DISP_CMD_LAYER_ENHANCE_ON after opening Video Layer."); \
    NX_FLAG(0, enhancementOff, "Always issue DISP_CMD_LAYER_ENHANCE_OFF after opening Video Layer."); \
    NX_INT_PARAM(0, surfaceSleepUs, "Delay before switching to next surface, 0.."); \
}

#include "flag_config_c.h"
