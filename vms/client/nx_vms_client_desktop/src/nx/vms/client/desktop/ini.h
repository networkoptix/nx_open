#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::desktop {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") {}

    NX_INI_STRING("", cloudHost,
        "Overrides the current Client's Cloud Host. Allows to connect to the Server that uses\n"
        "the specified Cloud Host.");
    NX_INI_FLAG(0, developerMode, "");
    NX_INI_FLAG(0, developerGuiPanel, "Enables developer GUI panel (WARNING: can be very slow).");

    NX_INI_FLAG(0, ignoreBetaWarning, "Hides beta version warning.");
    NX_INI_FLAG(0, enableEntropixEnhancer, "Enables Entropix image enhancement controls.");
    NX_INI_STRING("http://96.64.226.250:8888/image", entropixEnhancerUrl,
        "URL of Entropix image enhancement API.");
    NX_INI_FLAG(0, enableUnlimitedZoom,
        "Allows to zoom in to a very big scale by disabling the default zoom limitation.");
    NX_INI_FLAG(0, showVideoQualityOverlay, "");

    NX_INI_FLAG(0, enableOldAnalyticsController,
        "Enables old analytics controller (zoom-window based).");
    NX_INI_FLAG(0, demoAnalyticsDriver, "Enables demo analytics driver.");
    NX_INI_INT(0, demoAnalyticsProviderTimestampPrecisionUs, "");
    NX_INI_INT(4, demoAnalyticsProviderObjectsCount,
        "Number of objects generated by demo analytics provider.");
    NX_INI_STRING("", demoAnalyticsProviderObjectDescriptionsFile,
        "File containing descriptions for objects generated by demo analytics provider.");
    NX_INI_FLAG(0, externalMetadata, "Whether to use external metadata for local files.");
    NX_INI_FLAG(0, allowCustomArZoomWindows,
        "Allows zoom windows to have custom aspect ratio.\n"
        "\n"
        "When disabled, the acpect ratio of the zoom window is the same as the aspect ratio of\n"
        "the main camera window. If enabled, allows the user to specify the aspect ratio of the\n"
        "zoom window.");
    NX_INI_INT(500, analyticsVideoBufferLengthMs,
        "Length of the video buffer used when Analytics is enabled on a camera, milliseconds.\n"
        "\n"
        "Increases the video latency, but allows to work with the metadata with the timestamps\n"
        "from the nearest past. The metadata with timestamps that are older than the\n"
        "(currentTime - analyticsVideoBufferLengthMs) is ignored (not shown in the Client).");
    NX_INI_FLAG(0, hideEnhancedVideo, "Hides enhanced video from the scene.");
    NX_INI_FLAG(1, enableDetectedObjectsInterpolation,
        "Allows the interpolation of the trajectories of the Analytics Objects between frames.\n"
        "\n"
        "If enabled, bounding boxes around the Analytics Objects are moving more smoothly.");
    NX_INI_FLAG(0, displayAnalyticsDelay,
        "Whether to add a delay label to Analytics Object description.\n"
        "\n"
        "Allows to see the Analytics latency in the Desktop Client.");
    NX_INI_FLAG(0, displayAnalyticsEnginesInResourceTree,
        "Displays Analytics Engine items in the Resource tree.");
    NX_INI_FLAG(0, debugThumbnailProviders, "Enables debug mode for thumbnail providers.");
    NX_INI_FLAG(0, ignoreZoomWindowConstraints,
        "Whether to ignore the constrains for a zoom region.\n"
        "\n"
        "The default constraints are 0.1...0.9 of the linear size of the main camera window.");
    NX_INI_FLAG(0, showDebugTimeInformationInRibbon,
        "Whether to show extra timestamp information in event ribbon.");
    NX_INI_FLAG(0, showPreciseItemTimestamps,
        "Whether to show precise timestamps in camera window.\n"
        "\n"
        "If enabled, current time in a camera window is shown in microseconds since epoch\n"
        "together with the default human-readable date-time representation.");
    NX_INI_FLAG(0, massSystemUpdateDebugInfo,
        "Whether to show additional debug information for experimental update system.");
    NX_INI_FLAG(0, massSystemUpdateClearDownloads,
        "Forces the Client to remove downloaded data for system updates.");
    NX_INI_FLAG(0, forceCompatibilityMode,
        "Forces the compatibility mode dialog to appear when connecting to the system.");
    NX_INI_FLAG(0, allowOsScreenSaver,
        "Allows the OS to enable a screensaver when the user is not active.");
    NX_INI_FLAG(0, enableWebKitDeveloperExtras,
        "Enables WebKit developer tools such as Inspector.\n"
        "\n"
        "The tools will become available in built-in browser windows. ATTENTION: Activation\n"
        "of this setting may result in unstable behavior of the built-in browser.");
    NX_INI_FLAG(1, enableWebKitPlugins,
        "Enables WebKit NPAPI plugins (Flash, Java, etc.).");
    NX_INI_FLAG(1, modalServerSetupWizard,
        "Whether to show Server's setup wizard dialog in a modal window.");
    NX_INI_FLAG(0, enableTimelineScreenshotCursor,
        "Allows to show the screenshot above the timeline when mouse pointer hovers over the\n"
        "timeline.\n"
        "\n"
        "Is currently used for demo purposes only (does not work in some specific scenarios).");
    NX_INI_FLAG(1, enableWatermark,
        "Enables watermarks preview and setup.");
    NX_INI_FLAG(1, enableCaseExport,
        "Enables case export.");
    NX_INI_FLAG(1, enableSessionTimeout,
        "Enables admin-configurable absolute session timeout.");
    NX_INI_STRING("press", passwordPreviewActivationMode,
        "Specifies one of password preview activation modes: \"press\", \"hover\" or \"toggle\".");
    NX_INI_FLAG(1, redesignedTimeSynchronization,
        "Enables redesigned time synchronization widget in the System Adminstration dialog.");
    NX_INI_FLAG(1, automaticFilterByTimelineSelection,
        "Allows to automatically switch Right Panel time selection to \"Selected on Timeline\"\n"
        "mode when such selection exists.");
    NX_INI_FLAG(0, raiseCameraFromClickedTile,
        "Enables raising a camera when Right Panel camera-related tile is clicked.");
    NX_INI_INT(30, rightPanelPreviewReloadDelay,
        "Right Panel preview reload delay in seconds after receiving \"NO DATA\" (0 to disable).");
    NX_INI_FLAG(0, exclusiveMotionSelection,
        "Whether selecting a motion search region on a camera clears motion selection for other\n"
        "cameras on a layout.");
    NX_INI_FLAG(0, allowDeleteLocalFiles,
        "Allows to delete local files from the context menu.\n"
        "\n"
        "Is currently used for demo purposes only (does not work in some specific scenarios).");
    NX_INI_FLAG(1, startPlaybackOnTileNavigation,
        "Whether to start the playback if the timeline navigation occured after Right Panel tile\n"
        "click or double click.");
    NX_INI_FLAG(0, systemUpdateProgressInformers,
        "Whether to show Right Panel progress informers during System Update (unfinished\n"
        "functionality).");
    NX_INI_FLAG(0, compatibilityIsMediaPaused,
        "Enables a check if all sync play items are paused upon every request.");
    NX_INI_STRING("", autoUpdatesCheckChangesetOverride,
        "Background updates check will use this changeset instead of \"latest\".");
    NX_INI_INT(0, massSystemUpdateWaitForServerOnlineSecOverride,
        "Time to wait until Server goes online in seconds. Default value is used when set to 0.");
    NX_INI_INT(0, tilePreviewLoadDelayOverrideMs,
        "Tiles in the Right Panel will request previews only after this period (in milliseconds)\n"
        "after appearing. 0 means default value (100 ms).");
    NX_INI_INT(750, tilePreviewLoadIntervalMs,
        "Right Panel tiles will not request previews more often than this period, milliseconds.");
    NX_INI_FLAG(0, enableSyncedChunksForExtraContent,
        "Whether to show motion and analytics chunks in the synced area of the timeline.");
    NX_INI_INT(0, clientWebServerPort,
        "Enables web server to remotely control the Nx Client operation; port should be in range\n"
        "1..65535 (typically 7012) to enable; 0 means disabled.");
    NX_INI_INT(1000, storeFrameTimePoints,
        "Number of frame timestamps stored by the Client. Used in Functional Tests for fps\n"
        "measurement.");
    NX_INI_INT(1024, rightPanelMaxThumbnailWidth,
        "Maximum image width, in pixels, that Right Panel can request from a server.");
    NX_INI_INT(0, backgroupdUpdateCheckPeriodOverrideSec,
        "Period to check for new updates, in seconds. Set to zero to use built-in value.");
    NX_INI_FLAG(1, rightPanelHoverPreviewCrop,
        "Whether mouse hover toggles crop mode on Right Panel tiles and tooltips.");
    NX_INI_FLAG(0, pluginInformationInServerSettings,
        "Show information about installed plugin libraries in Server Settings.");
    NX_INI_FLAG(0, cacheLiveVideoForRightPanelPreviews,
        "Cache live video to obtain right panel previews without querying the server.");
    NX_INI_INT(0, globalLiveVideoCacheLength,
        "Global live video cache length, in seconds. Set to zero to use built-in value.");
    NX_INI_INT(0, connectTimeoutMs,
        "Timeout (in milliseconds) for waiting initial resources message from the server.\n"
        "If exceeded then connections is dropped to avoid infinite UI \"Loading...\" state. 0 means disabled.");
    NX_INI_FLAG(0, enableVSyncWorkaround,
        "Always limit frame rate to approximately 60 fps, even if VSync is disabled.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms::client::desktop
