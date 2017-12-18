#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace client {
namespace desktop {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") {}

    NX_INI_FLAG(0, enableEntropixEnhancer, "Enable Entropix image enhancement controls.");
    NX_INI_STRING("http://96.64.226.250:8888/image",
        entropixEnhancerUrl, "URL of Entropix image enhancement API.");
    NX_INI_FLAG(0, enableUnlimitedZoom, "Enable unlimited zoom feature.");
    NX_INI_FLAG(0, showVideoQualityOverlay, "Show video quality overlay.");
    NX_INI_FLAG(0, enableAnalytics, "Enable analytics engine.");
    NX_INI_FLAG(0, demoAnalyticsDriver, "Enable demo analytics driver.");
    NX_INI_FLAG(0, externalMetadata, "Use external metadata for local files.");
    NX_INI_FLAG(0, allowCustomArZoomWindows, "Allow zoom windows to have custom aspect ratio.");
    NX_INI_FLAG(0, hideEnhancedVideo, "Hide enhanced video from the scene.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace desktop
} // namespace client
} // namespace nx
