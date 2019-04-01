#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_server.ini") { reload(); }

    NX_INI_FLAG(0, verboseAutoRequestForwarder, "Set log level to Verbose for AutoRequestForwarder.");
    NX_INI_FLAG(0, ignoreApiModuleInformationInAutoRequestForwarder, "");
    NX_INI_FLAG(0, enableApiDebug, "Enable /api/debug RestAPI method.");

    NX_INI_FLAG(1, enableMetadataProcessing, "Enable processing data from metadata plugins.");
    NX_INI_FLAG(0, analyzeKeyFramesOnly, "Use only key frames for metadata plugins.");
    NX_INI_FLAG(0, analyzeSecondaryStream, "Use secondary stream for analytics instead of primary.");
    NX_INI_FLAG(1, enablePersistentAnalyticsDeviceAgent,
        "Don't recreate analytics DeviceAgents on resource changes (workaround of libtegra_video.so bug).");

    NX_INI_FLAG(0, forceLiteClient, "Force Lite Client for this server.");

    NX_INI_INT(67000, liveStreamCacheForPrimaryStreamMinSizeMs,
        "Lower bound of Live Stream Cache size for primary stream, milliseconds.");
    NX_INI_INT(100000, liveStreamCacheForPrimaryStreamMaxSizeMs,
        "Upper bound of Live Stream Cache size for primary stream, milliseconds.");
    NX_INI_INT(67000, liveStreamCacheForSecondaryStreamMinSizeMs,
        "Lower bound of Live Stream Cache size for secondary stream, milliseconds.");
    NX_INI_INT(100000, liveStreamCacheForSecondaryStreamMaxSizeMs,
        "Upper bound of Live Stream Cache size for secondary stream, milliseconds.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}
