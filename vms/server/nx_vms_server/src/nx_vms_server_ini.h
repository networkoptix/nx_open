#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_server.ini") { reload(); }

    NX_INI_FLAG(0, enableMallocStatisticsLogging,
        "Enables info-level logging of malloc statistics. Only Linux is supported.");
    NX_INI_FLAG(0, verboseAutoRequestForwarder,
        "Sets log level to Verbose for AutoRequestForwarder.");
    NX_INI_FLAG(0, ignoreApiModuleInformationInAutoRequestForwarder, "");
    NX_INI_FLAG(0, enableApiDebug, "Enables /api/debug RestAPI method.");

    NX_INI_FLAG(0, forceLiteClient, "Force Lite Client for this server.");

    #define NX_VMS_SERVER_INI_LIVE_STREAM_CACHE_HELP \
        "Live Stream Cache allows Desktop Client's Right Panel receive adequate thumbnails of\n" \
        "the detected Objects and Events. It is needed because frames in the video archive are\n" \
        "inaccessible for ~1 minute since the moment of recording. Beware that increasing the\n" \
        "Live Stream Cache size increases the RAM usage."

    NX_INI_INT(67000, liveStreamCacheForPrimaryStreamMinSizeMs,
        "Lower bound of Live Stream Cache size for primary stream, milliseconds.\n\n"
        NX_VMS_SERVER_INI_LIVE_STREAM_CACHE_HELP);
    NX_INI_INT(100000, liveStreamCacheForPrimaryStreamMaxSizeMs,
        "Upper bound of Live Stream Cache size for primary stream, milliseconds.\n\n"
        NX_VMS_SERVER_INI_LIVE_STREAM_CACHE_HELP);
    NX_INI_INT(67000, liveStreamCacheForSecondaryStreamMinSizeMs,
        "Lower bound of Live Stream Cache size for secondary stream, milliseconds.\n\n"
        NX_VMS_SERVER_INI_LIVE_STREAM_CACHE_HELP);
    NX_INI_INT(100000, liveStreamCacheForSecondaryStreamMaxSizeMs,
        "Upper bound of Live Stream Cache size for secondary stream, milliseconds.\n\n"
        NX_VMS_SERVER_INI_LIVE_STREAM_CACHE_HELP);

    #undef NX_VMS_SERVER_INI_LIVE_STREAM_CACHE_HELP

    NX_INI_STRING("", currentOsVariantOverride,
        "Override the detected OS variant value (e.g. \"ubuntu\").");
    NX_INI_STRING("", currentOsVariantVersionOverride,
        "Override the detected OS variant version value (e.g. \"16.04\").");

    NX_INI_INT(60000, autoUpdateInstallationDelayMs,
        "After this delay server will start update installation automatically if detects that\n"
        "it was supposed to install the update.");

    NX_INI_FLAG(0, disableArchiveIntegrityWatcher, "Disables media files integrity check.");

    NX_INI_INT(100, stopTimeoutS, "Timeout to wait on server stop before crash.");

    NX_INI_FLAG(0, pushNotifyOnPopup, "Sends push notifications on popup actions.");
    NX_INI_FLAG(0, pushNotifyCommonUtfIcon, "Enables UTF icon for common level messages.");
    NX_INI_STRING("", pushNotifyImageUrl, "Overrides imageUrl for all push notifications.");
    NX_INI_STRING(R"json({"priority": "high", "mutable_content": true})json", pushNotifyOptions, "");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}
