#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::desktop {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") {}

    NX_INI_STRING("", cloudHost, "Overridden Cloud Host.");
    NX_INI_FLAG(0, developerMode, "Developer mode.");
    NX_INI_FLAG(0, developerGuiPanel, "Enable developer gui panel (WARNING: can be very slow).");

    NX_INI_FLAG(0, ignoreBetaWarning, "Hide beta version warning.");
    NX_INI_FLAG(0, enableEntropixEnhancer, "Enable Entropix image enhancement controls.");
    NX_INI_STRING("http://96.64.226.250:8888/image", entropixEnhancerUrl,
        "URL of Entropix image enhancement API.");
    NX_INI_FLAG(0, enableUnlimitedZoom, "Enable unlimited zoom feature.");
    NX_INI_FLAG(0, showVideoQualityOverlay, "Show video quality overlay.");

    NX_INI_FLAG(0, enableOldAnalyticsController,
        "Enable old analytics controller (zoom-window based).");
    NX_INI_FLAG(0, demoAnalyticsDriver, "Enable demo analytics driver.");
    NX_INI_INT(0, demoAnalyticsProviderTimestampPrecisionUs,
        "Timestamp precision of demo analytics provider.");
    NX_INI_INT(4, demoAnalyticsProviderObjectsCount,
        "Count of objects generated by demo analytics provider.");
    NX_INI_STRING("", demoAnalyticsProviderObjectDescriptionsFile,
        "File containing descriptions for obects generated by demo analytics provider.");
    NX_INI_FLAG(0, externalMetadata, "Use external metadata for local files.");
    NX_INI_FLAG(0, allowCustomArZoomWindows, "Allow zoom windows to have custom aspect ratio.");
    NX_INI_INT(500, analyticsVideoBufferLengthMs,
        "Video buffer length when analytics mode is enabled on a camera.");
    NX_INI_FLAG(0, hideEnhancedVideo, "Hide enhanced video from the scene.");
    NX_INI_FLAG(1, enableDetectedObjectsInterpolation,
        "Allow interpolation of detected objects between frames.");
    NX_INI_FLAG(0, displayAnalyticsDelay, "Add delay label to detected object description.");
    NX_INI_FLAG(0, displayAnalyticsEnginesInResourceTree,
        "Display analytics engine items in the resource tree.");
    NX_INI_FLAG(0, debugThumbnailProviders, "Enable debug mode for thumbnail providers.");
    NX_INI_FLAG(0, ignoreZoomWindowConstraints, "Ignore constrains for a zoom region.");
    NX_INI_FLAG(0, showDebugTimeInformationInRibbon,
        "Show extra timestamp information in event ribbon.");
    NX_INI_FLAG(0, showPreciseItemTimestamps, "Show precise timestamps on scene items.");
    NX_INI_FLAG(0, massSystemUpdateDebugInfo,
        "Show additional debug information for experimental update system.");
    NX_INI_FLAG(0, massSystemUpdateClearDownloads,
        "Forces client to remove downloaded data for system updates.");
    NX_INI_FLAG(0, forceCompatibilityMode,
        "Forces compatibility mode dialog to appear during connect to the system.");
    NX_INI_FLAG(0, allowOsScreenSaver, "Allow OS to enable screensaver when user is not active.");
    NX_INI_FLAG(0, enableWebKitDeveloperExtras, "Enable WebKit developer tools like Inspector.");
    NX_INI_FLAG(1, enableWebKitPlugins, "Enable WebKit NPAPI plugins (Flash, Java, etc.)");
    NX_INI_FLAG(1, modalServerSetupWizard, "Server setup wizard dialog is a modal window.");
    NX_INI_FLAG(0, enableTimelineScreenshotCursor,
        "Show screenshot cursor when hovering above timeline.");
    NX_INI_FLAG(1, enableWatermark, "Enable watermarks preview and setup.");
    NX_INI_FLAG(1, enableCaseExport, "Enable case export.");
    NX_INI_FLAG(1, enableSessionTimeout,
        "Enable admin-configurable absolute session timeout.");
    NX_INI_STRING("press", passwordPreviewActivationMode,
        "Password preview activation mode: \"press\", \"hover\" or \"toggle\".");
    NX_INI_FLAG(1, redesignedTimeSynchronization,
        "Redesigned time synchronization widget in the System Adminstration dialog.");
    NX_INI_FLAG(1, automaticFilterByTimelineSelection,
        "Automatically switch Right Panel time selection to \"Selected on Timeline\" mode when\n"
        "selection exists.");
    NX_INI_FLAG(0, raiseCameraFromClickedTile,
        "Raise camera after selecting it when Right Panel camera-related tile is clicked.");
    NX_INI_INT(30, rightPanelPreviewReloadDelay,
        "Right Panel preview reload delay in seconds after receiving \"NO DATA\" (0 to disable).");
    NX_INI_FLAG(0, exclusiveMotionSelection,
        "Whether selecting a motion search region on a camera clears motion selection on other\n"
        "cameras on the layout.");
    NX_INI_FLAG(0, allowDeleteLocalFiles,
        "Allow delete local files from the context menu.");
    NX_INI_FLAG(1, startPlaybackOnTileNavigation,
        "Start playback if timeline navigation occured after Right Panel tile click or double\n"
        "click.");
    NX_INI_FLAG(0, systemUpdateProgressInformers,
        "Show Right Panel progress informers during System Update (unfinished functionality).");
    NX_INI_FLAG(0, compatibilityIsMediaPaused,
        "Check if all sync play items are paused at every request.");
    NX_INI_STRING("", autoUpdatesCheckChangesetOverride,
        "Background updates check will use this changeset instead of \"latest\".");
    NX_INI_INT(0, massSystemUpdateWaitForServerOnlineOverride,
        "Period to wait until the Server goes online, in seconds. Set to zero to use the\n"
        "built-in value.");
    NX_INI_INT(0, tilePreviewLoadDelayOverrideMs,
        "Tiles in the right panel will request previews only after this time, in milliseconds,\n"
        "after appearing. 0 means default value (100 ms).");
    NX_INI_INT(750, tilePreviewLoadIntervalMs,
        "Right Panel tiles will not request previews more often than this time, in milliseconds.");
    NX_INI_FLAG(0, enableSyncedChunksForExtraContent,
        "Show motion and analytics chunks at synced area of the timeline.");
    NX_INI_INT(0, clientWebServerPort,
        "Enables web server to remote control NX client operation. Set port value 1..65535\n"
        "(typically 7012) to enable.");
    NX_INI_INT(1000, storeFrameTimePoints,
        "Makes client to remember this number of last frame paint time points for using in FT.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms::client::desktop
