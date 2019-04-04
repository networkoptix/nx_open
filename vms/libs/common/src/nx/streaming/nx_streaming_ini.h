#pragma once

#include <nx/kit/ini_config.h>

// TODO: Rename and put into namespaces when nx_streaming is extracted from lib common.
struct NxStreamingIniConfig: public nx::kit::IniConfig
{
    NxStreamingIniConfig(): nx::kit::IniConfig("nx_streaming.ini") { reload(); }

    NX_INI_STRING("", analyticsMetadataLogFilePrefix,
        "If not empty, metadata will be logged to this file.");

    NX_INI_INT(0, unloopCameraPtsWithModulus,
        "If not 0, pts from camera is \"unlooped\" to be monotonous and close to \"now\".");

    NX_INI_FLAG(
        1,
        enableTimeCorrection,
        "Enables time correction if timestamp difference between subsequent frames is too small.");

    NX_INI_INT(
        1000,
        resyncTresholdMs,
        "If the difference between camera time and server time is bigger than the value of this\n"
        "setting an offset between the clocks of the camera and the server is recalculated");

    NX_INI_INT(
        5000,
        streamsSyncThresholdMs,
        "If the difference between the camera primary and secondary stream clocks is bigger\n"
        "than the value of this settings then the secondary stream time offset is recalculated");

    NX_INI_INT(
        10000,
        forceCameraTimeThresholdMs,
        "If the difference between the camera clock and the server clock is bigger than this\n"
        "value then the server will bind the camera clock to the server one. Otherwise the\n"
        "camera clock will be used");
};

inline NxStreamingIniConfig& nxStreamingIni()
{
    static NxStreamingIniConfig ini;
    return ini;
}
