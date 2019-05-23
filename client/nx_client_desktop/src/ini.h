#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace client {
namespace desktop {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") {}

    NX_INI_STRING("", cloudHost, "Overridden Cloud Host");
    NX_INI_FLAG(0, ignoreBetaWarning, "Hide beta version warning");
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
    NX_INI_FLAG(1, enableWearableCameras, "Enable wearable cameras.");
    NX_INI_FLAG(0, debugThumbnailProviders, "Enable debug mode for thumbnail providers");
    NX_INI_FLAG(0, allowOsScreenSaver, "Allow OS to enable screensaver when user is not active.");
    NX_INI_FLAG(0, enableWebKitDeveloperExtras, "Enable WebKit developer tools like Inspector.");
    NX_INI_FLAG(1, modalServerSetupWizard, "Server setup wizard dialog is a modal window.");
    NX_INI_FLAG(0, enableWatermark, "Enable watermarks preview and setup.");
    NX_INI_FLAG(0, enableCaseExport, "Enable case export.");
    NX_INI_FLAG(0, enableGdiTrace, "Enable tracing of GDI object allocation.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace desktop
} // namespace client
} // namespace nx
