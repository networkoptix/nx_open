// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

// TODO: Rename and put into namespaces when nx_streaming is extracted from lib common.
struct NxStreamingIniConfig: public nx::kit::IniConfig
{
    NxStreamingIniConfig(): nx::kit::IniConfig("nx_streaming.ini") { reload(); }

    NX_INI_STRING("", analyticsMetadataLogFilePrefix,
        "If not empty, metadata will be logged to this file.");

    NX_INI_FLAG(
        1,
        enableTimeCorrection,
        "Enables time correction if timestamp difference between subsequent frames is too small.");

    NX_INI_INT(
        5000,
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

    NX_INI_INT(
        5000,
        maxExpectedMetadataDelayMs,
        "For metadata arriving via a side channel (i.e not via the RTSP connection), when\n"
        "RTSP provides no absolute timing information, if the difference between the camera\n"
        "clock used for metadata and the server clock is bigger than this value plus\n"
        "forceCameraTimeThresholdMs then the server will bind the camera metadata clock to the\n"
        "server one. Otherwise the camera metadata clock will be used");
};

NX_VMS_COMMON_API NxStreamingIniConfig& nxStreamingIni();
