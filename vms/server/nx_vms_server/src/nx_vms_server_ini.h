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

    NX_INI_FLAG(0, forceLiveCacheForPrimaryStream, "Always cache primary stream frames in liveCache.");
    NX_INI_FLAG(0, forceLiteClient, "Force Lite Client for this server.");

    NX_INI_FLAG(0, allowMtDecoding,
        "Allow multithreading decoding of video when software motion detection is enabled.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}
