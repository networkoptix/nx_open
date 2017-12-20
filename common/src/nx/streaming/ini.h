#pragma once

#include <nx/kit/ini_config.h>

// TODO: Rename and put into namespaces when nx_streaming is extracted from lib common.
struct NxStreamingIniConfig: public nx::kit::IniConfig
{
    NxStreamingIniConfig(): nx::kit::IniConfig("nx_streaming.ini") { reload(); }

    NX_INI_STRING("", analyticsMetadataLogFilePrefix,
        "If not empty, metadata will be logged to this file.");

    NX_INI_FLAG(0, disableTimeCorrection, "Disable correcting bad-looking frame timestamps.");
};

inline NxStreamingIniConfig& nxStreamingIni()
{
    static NxStreamingIniConfig ini;
    return ini;
}
