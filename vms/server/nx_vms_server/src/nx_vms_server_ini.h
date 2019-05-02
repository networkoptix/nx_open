#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_server.ini") { reload(); }

    NX_INI_FLAG(0, verboseAutoRequestForwarder, 
        "Sets log level to Verbose for AutoRequestForwarder.");
    NX_INI_FLAG(0, ignoreApiModuleInformationInAutoRequestForwarder, "");
    NX_INI_FLAG(0, enableApiDebug, "Enables /api/debug RestAPI method.");

    NX_INI_FLAG(1, enableMetadataProcessing, "Enables processing data from metadata plugins.");
    NX_INI_FLAG(0, analyzeKeyFramesOnly, "Whether to use only key frames for metadata plugins.");
    NX_INI_FLAG(0, analyzeSecondaryStream, 
        "Whether to use secondary stream for Analytics instead of primary.\n"
        "\n"
        "Enabling may reduce the CPU usage on the Server, but may cause the degradation of\n"
        "detection quality, especially for plugins that need high-resolution frames, such as\n"
        "face detection and plugins that detect fast-moving objects (fps rate on the secondary\n"
        "stream is usually lower than on the primary stream.");
    NX_INI_FLAG(1, enablePersistentAnalyticsDeviceAgent,
        "Disables recreating of Analytics DeviceAgents on resource changes (workaround of\n"
        "libtegra_video.so bug).");

    NX_INI_FLAG(0, forceLiteClient, "Force Lite Client for this server.");
	
    NX_INI_FLAG(0, allowMtDecoding,
        "Allow multithreading decoding of video when software motion detection is enabled.");


    NX_INI_INT(67000, liveStreamCacheForPrimaryStreamMinSizeMs,
        (std::string("Lower bound of Live Stream Cache size for primary stream, milliseconds.\n\n")
        + kLiveStreamCacheHelp).c_str());
    NX_INI_INT(100000, liveStreamCacheForPrimaryStreamMaxSizeMs,
        (std::string("Upper bound of Live Stream Cache size for primary stream, milliseconds.\n\n")
        + kLiveStreamCacheHelp).c_str());
    NX_INI_INT(67000, liveStreamCacheForSecondaryStreamMinSizeMs,
        (std::string("Lower bound of Live Stream Cache size for secondary stream, milliseconds.\n\n")
        + kLiveStreamCacheHelp).c_str());
    NX_INI_INT(100000, liveStreamCacheForSecondaryStreamMaxSizeMs,
        (std::string("Upper bound of Live Stream Cache size for secondary stream, milliseconds.\n\n")
        + kLiveStreamCacheHelp).c_str());

private:        
    static constexpr char kLiveStreamCacheHelp[] =
        "Live Stream Cache allows Desktop Client's Right Panel receive adequate thumbnails of\n"
        "the detected Objects and Events. It is needed because frames in the video archive are\n"
        "inaccessible for ~1 minute since the moment of recording. Beware that increasing the\n"
        "Live Stream Cache size increases the RAM usage.";
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}
