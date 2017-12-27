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

    NX_INI_FLAG(0, unifiedEventPanel, "Use unified event panel instead of old notifications panel.");
    NX_INI_FLAG(1, enableAnalytics, "Enable analytics engine");
    NX_INI_FLAG(0, enableOldAnalyticsController, "Enable old analytics controller (zoom-window based).");
    NX_INI_FLAG(0, demoAnalyticsDriver, "Enable demo analytics driver.");
    NX_INI_INT(0, demoAnalyticsProviderTimestampPrecisionUs, "Timestamp precision of demo analytics provider.");
    NX_INI_FLAG(0, externalMetadata, "Use external metadata for local files.");
    NX_INI_FLAG(0, allowCustomArZoomWindows, "Allow zoom windows to have custom aspect ratio.");
    NX_INI_INT(0, forcedVideoBufferLengthUs, "If non-zero, playing video will not start until video buffer is filled by this length.");
    NX_INI_INT(0, additionalVideoBufferLengthUs, "When video is buffering the buffer is filled for forcedVideoBufferLengthUs + this length.");
    NX_INI_FLAG(0, hideEnhancedVideo, "Hide enhanced video from the scene.");
    NX_INI_FLAG(0, redesignedCameraSettingsDialog, "Enable redesigned camera settings dialog.");
    NX_INI_FLAG(1, enableDetectedObjectsInterpolation, "Allow interpolation of detected objects between frames.");
    NX_INI_FLAG(0, displayAnalyticsDelay, "Add delay label to detected object description.");
    NX_INI_FLAG(0, enableProgressInformers, "Enable global operation progress informers in the notification panel.");
    NX_INI_FLAG(0, enableDeviceSearch, "Enable reworked device search dialog");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace desktop
} // namespace client
} // namespace nx
