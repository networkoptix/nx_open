// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_client_core.ini") { reload(); }

    // VMS-30347.
    NX_INI_INT(8, maxLastConnectedTilesStored,
        "[Support] Maximum last connected systems tiles stored on the Welcome Screen");

    NX_INI_FLAG(false, doNotPingCloudSystems,
        "[Dev] Disable cloud system pings, so desktop client will rely on cloud status only");

    NX_INI_STRING("", dumpGeneratedIconsTo,
        "[Dev] Dump icons, generated from svg, to a given folder.");

    NX_INI_INT(0, systemsHideOptions,
        "[Dev] Hide systems, bitwise combination of flags:\n"
        " * 1 - Incompatible systems.\n"
        " * 2 - Not connectable cloud systems.\n"
        " * 4 - Compatible systems which require compatibility mode.\n"
        " * 8 - All systems except those on localhost.\n"
        " * 16 - Local auto-discovered and not connected before.\n"
    );

    NX_INI_INT(30000, cameraDataLoadingIntervalMs,
        "[Dev] Interval, at which the Client loads camera data chunks.");

    NX_INI_STRING("", rootCertificatesFolder,
        "Path to a folder with user-provided certificates. If set, all *.pem and *.crt files\n"
        "from this folder are used as trusted root certificates together with the system ones.");

    NX_INI_STRING("", colorTheme,
        "[Dev] Color theme to use. If blank, the original theme is used. The possible values\n"
        "are: dark_blue, dark_green, dark_orange, gray_orange, gray_white, rainbow, generated\n");

    NX_INI_FLAG(false, invertColors,
        "[Dev] If set to true, dark and light colors of the generated Color Scheme are inverted.\n");

    NX_INI_STRING("#8a40bf", primaryColor,
        "[Dev] Primary color of the generated Color Scheme in hex. If the colorTheme isn't set to\n"
        "'generated', this value is ignored.");

    NX_INI_STRING("", backgroundColor,
        "[Dev] Background color of the generated Color Scheme in hex, e.g. '#cc7733'.\n"
        "If set, its Hue component is used to generate the dark and light colors of the Color Scheme.\n"
        "Otherwise, the Hue component of the primary color is used.\n");

    NX_INI_FLOAT(0.15, backgroundSaturation,
        "[Dev] Saturation of the background (dark_ and light_) colors of generated color scheme.\n"
        "Should be a number between 0 and 1. If set to zero, the color scheme is grayscale.\n");

    NX_INI_FLOAT(0.02, darkStep,
        "[Dev] Lightness step between two sequential dark colors of generated color scheme.\n"
        "Should be a number between 0 and 0.06, where the value 0.02 means 2%.");

    NX_INI_FLOAT(0.03, lightStep,
        "[Dev] Lightness step between two sequential light colors of generated color scheme.\n"
        "Should be a number between 0 and 0.06, where the value 0.03 means 3%.");

//    NX_INI_STRING("", colorSubstitutions,
//        "[Dev] A path to .json file with additional colors used to overwrite the default or the generated\n"
//        "Color Scheme.\n");
};

NX_VMS_CLIENT_CORE_API Ini& ini();

} // namespace nx::vms::client::core
