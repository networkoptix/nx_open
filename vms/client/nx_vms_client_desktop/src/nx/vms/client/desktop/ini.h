#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::desktop {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") { reload(); }

    // ---------------------------------------------------------------------------------------------
    // Developers' section
    // Flags here can be removed by developers' decision.

    NX_INI_FLAG(0, developerMode,
        "[Dev] Enable developer mode.");

    NX_INI_FLAG(0, profilerMode,
        "[Dev] Enables client profiling options.");

    NX_INI_STRING("", cloudHost,
        "[Dev] Overrides the current Client's Cloud Host. Allows to connect to the Server that \n"
        "uses the specified Cloud Host.");

    NX_INI_FLAG(1, limitFrameRate,
        "[Dev] Limits client frame rate to the maximum of 60 fps. Forces VSync on the video \n"
        "drivers.");

    NX_INI_FLAG(0, enableVSyncWorkaround,
        "[Dev] Always limit frame rate to approximately 60 fps, even if VSync is disabled.");

    NX_INI_FLAG(0, developerGuiPanel,
        "[Dev] Enables on-screen Qt debug panel (WARNING: can be very slow).");

    NX_INI_FLAG(0, showVideoQualityOverlay,
        "[Dev] Display green overlay over video if HQ or red overlay if LQ.");

    NX_INI_FLAG(0, debugThumbnailProviders,
        "[Dev] Enables debug mode for the thumbnail providers.");

    NX_INI_STRING("", displayAnalyticsObjectsDebugInfo,
        "[Dev] Whether to add a debug info label to Analytics Object description.\n"
        "Allows to see the Analytics Objects details in the Desktop Client. Use \"all\" to\n"
        "display all available metadata. More precise filtering is available by combining the\n"
        "following options:\n"
        " * \"id\" - Track id,\n"
        " * \"delay\" - Delay between actual timestamp and object timestamp in milliseconds,\n"
        " * \"actual_ts\" - Current timestamp,\n"
        " * \"actual_rect\" - Interpolated rect if \"enableObjectMetadataInterpolation\" is on,\n"
        " * \"object_ts\" - Original object timestamp,\n"
        " * \"object_rect\" - Original object rect,\n"
        " * \"future_ts\" - Future object timestamp,\n"
        " * \"future_rect\" - Future object rect.\n"
        "Fields can be combined using space, comma or any other separator."
    );

    NX_INI_STRING("", debugAnalyticsVideoOverlayFromLogFile,
        "[Dev] Display analytics debug overlay, based on the log file parsing.\n"
        "Full path to the log file must be used here.");

    NX_INI_FLAG(0, showDebugTimeInformationInRibbon,
        "[Dev] Whether to show extra timestamp information in event ribbon.");

    NX_INI_FLAG(0, showPreciseItemTimestamps,
        "[Dev] Whether to show precise timestamps in camera window.\n"
        "If enabled, current time in a camera window is shown in microseconds since epoch\n"
        "together with the default human-readable date-time representation.");

    NX_INI_STRING("", updateFeedUrl,
        "[Dev] Overrides URL update server. Leave empty value to use built-in path.");

    NX_INI_FLAG(0, massSystemUpdateDebugInfo,
        "[Dev] Whether to show additional debug information for experimental update system.");

    NX_INI_FLAG(0, massSystemUpdateClearDownloads,
        "[Dev] Forces the Client to remove downloaded data for system updates.");

    NX_INI_FLAG(0, forceCompatibilityMode,
        "[Dev] Forces the compatibility mode dialog to appear when connecting to the system.");

    NX_INI_FLAG(1, modalServerSetupWizard,
        "[Dev] Whether to show Server's setup wizard dialog in a modal window (default).\n"
        "Developers can change to non-modal for debugging of web-based components.");

    NX_INI_STRING("", autoUpdatesCheckChangesetOverride,
        "[Dev] Background updates check will use this changeset instead of \"latest\".");

    NX_INI_INT(0, massSystemUpdateWaitForServerOnlineSecOverride,
        "[Dev] Time to wait until Server goes online in seconds.\n"
        "Default value is used when set to 0.");

    NX_INI_INT(0, backgroupdUpdateCheckPeriodOverrideSec,
        "[Dev] Period to check for new updates, in seconds. Set to zero to use built-in value.");

    NX_INI_STRING("", dumpGeneratedIconsTo,
        "[Dev] Dump icons, generated from svg, to a given folder.");

    NX_INI_FLAG(0, enableGdiTrace,
        "[Dev] Enable tracing of GDI object allocation.");

    NX_INI_INT(5000, gdiTraceLimit,
        "[Dev] Number of GDI handles in use which triggers report creation.");

    NX_INI_FLAG(0, alwaysShowGetUpdateFileButton,
        "[Dev] Always show Get Update File button.");

    NX_INI_STRING("", currentOsVariantOverride,
        "[Dev] Override detected OS variant value (e.g. \"ubuntu\").");

    NX_INI_STRING("", currentOsVariantVersionOverride,
        "[Dev] Override detected OS variant version value (e.g. \"16.04\").");

    NX_INI_INT(0, tileHideOptions,
        "[Dev] Hide system tiles on welcome screen, bitwise combination of flags:\n"
        " * 1 - Incompatible systems.\n"
        " * 2 - Not connectable cloud systems.\n"
        " * 4 - Compatible systems which require compatibility mode.\n");

    NX_INI_FLAG(1, resetResourceTreeModelOnUserChange,
        "[Dev] Reset Resource Tree model during user session change.");

    NX_INI_FLAG(0, forceJsonConnection,
        "[Dev] Force desktop client use json data encoding");

    NX_INI_FLAG(0, ignoreSavedPreferredCloudServers,
        "[Dev] Ignore cloud server preference saved by \"Connect to this Server\" commands.");

    // ---------------------------------------------------------------------------------------------
    // Design section
    // Flags here can be removed when designers approve the resulting approach.

    // DESIGN-751
    NX_INI_STRING("press", passwordPreviewActivationMode,
        "[Design] Specifies one of password preview activation modes:\n"
        "\"press\", \"hover\" or \"toggle\".");

    NX_INI_FLAG(0, startPlaybackOnTileNavigation,
        "[Design] Whether to start the playback if the timeline navigation occured after Right\n"
        "Panel tile click or double click.");

    NX_INI_FLAG(0, enableSyncedChunksForExtraContent,
        "[Design] Whether to show merged bookmarks and analytics chunks in the synced area of the\n"
        "timeline.");

    // DESIGN-750
    NX_INI_FLAG(1, enableAnalyticsPlaybackMask,
        "[Design] Enable playback mode of analytics chunks only.");

    // VMS-18865, VMS-18866
    NX_INI_FLAG(1, allowMultipleClientInstances,
        "[Design] Whether client allowed to run in multiple instances (e.g. by desktop shortcut).");

    NX_INI_FLAG(0, oldPtzAimOverlay,
        "[Design] Use old-style circular aim overlay to operate PTZ pan and tilt.");

    // ---------------------------------------------------------------------------------------------
    // Features section
    // Flags here can be removed when QA approves the feature to be definitely present in the
    // current release.

    // Probably will be implemented in 4.2 or later.
    NX_INI_FLAG(0, displayAnalyticsEnginesInResourceTree,
        "[Feature] Displays Analytics Engine items in the Resource tree.");

    // VMS-10483
    NX_INI_FLAG(0, enableTimelineScreenshotCursor,
        "[Feature] Allows to show the screenshot above the timeline when mouse pointer hovers\n"
        "over the timeline.");

    // VMS-16691
    NX_INI_FLAG(0, allowDeleteLocalFiles,
        "[Feature] Allows to delete local files from the context menu.");

    // VMS-11893
    NX_INI_FLAG(0, systemUpdateProgressInformers,
        "[Feature] Whether to show Right Panel progress informers during System Update");

    // VMS-11543
    NX_INI_FLAG(0, pluginInformationInServerSettings,
        "[Feature] Show information about installed plugin libraries in Server Settings.");

    // VMS-13860. Implemented in 4.1. Should be removed in 4.2 if everything works well.
    NX_INI_FLAG(1, cacheLiveVideoForRightPanelPreviews,
        "[Feature] Cache live video to obtain right panel previews without querying the server.");

    // VMS-13860. Implemented in 4.1. Should be removed in 4.2 if everything works well.
    NX_INI_INT(1, globalLiveVideoCacheLength,
        "[Feature] Global live video cache length, in seconds. Set to zero to use built-in value.");

    // ---------------------------------------------------------------------------------------------
    // Support section.
    // Flags here can be removed when support engineers decide whether to discard the flag or move
    // it to the client settings permanently.

    NX_INI_INT(30, rightPanelPreviewReloadDelay,
        "[Support] Right Panel preview reload delay in seconds after receiving \"NO DATA\"\n"
        "(0 to disable).");

    NX_INI_INT(0, tilePreviewLoadDelayOverrideMs,
        "[Support] Tiles in the Right Panel will request previews only after this period after\n"
        "appearing. Value in milliseconds.");

    NX_INI_INT(0, tilePreviewLoadIntervalMs,
        "[Support] Right Panel tiles will not request previews more often than this period.\n"
        "Value in milliseconds.");

    NX_INI_INT(6, maxSimultaneousTilePreviewLoads,
        "[Support] Right Panel tab will not request simultaneously more previews than this number\n"
        "from standard arch (non-ARM) server. Valid range: [1, 15].");

    NX_INI_INT(3, maxSimultaneousTilePreviewLoadsArm,
        "[Support] Right Panel tab will not request simultaneously more previews than this number\n"
        "from ARM server. Valid range: [1, 5].");

    NX_INI_INT(1024, rightPanelMaxThumbnailWidth,
        "[Support] Maximum potential image width in pixels, that Right Panel can request from a\n"
        "server. Idea here that we need to request big image, so cropped analytics object looks\n"
        "sharp on the right panel preview.");

    NX_INI_INT(180000, connectTimeoutMs,
        "[Support] Timeout for waiting for the initial resource message from the Server.\n"
        "If exceeded, then the connection is dropped to avoid infinite UI \"Loading...\" state.\n"
        "Value in milliseconds, 0 means infinite timeout.");

    NX_INI_FLAG(0, delayRightPanelLiveAnalytics,
        "[Support] Prohibits showing right panel live analytics before corresponding frame appears on\n"
        "the camera if the camera is playing live");

    NX_INI_FLAG(0, debugDisableCameraThumbnails,
        "[Support] Disable camera thumbnail server requests for debugging and profiling purposes.\n"
        "Also disables Timeline Thumbnail Pane and Preview Search.");

    // ---------------------------------------------------------------------------------------------
    // CI section.
    // Flags here are used for the client functional unit tests.

    NX_INI_INT(0, clientWebServerPort,
        "[CI] Enables web server to remotely control the Nx Client operation; port should be in\n"
        "range 1..65535 (typically 7012) to enable; 0 means disabled.");

    NX_INI_STRING("", clientWebServerHost,
        "[CI] Listen address for local web server. It should contain a valid ip address.");

    NX_INI_INT(1000, storeFrameTimePoints,
        "[CI] Number of frame timestamps stored by the Client. Used in Functional Tests for fps\n"
        "measurement.");

    // ---------------------------------------------------------------------------------------------
    // Entropix demo feature section.

    NX_INI_FLAG(0, enableEntropixEnhancer,
        "[Entropix] Enables Entropix image enhancement controls.");

    NX_INI_STRING("http://96.64.226.250:8888/image", entropixEnhancerUrl,
        "[Entropix] URL of Entropix image enhancement API.");

    NX_INI_FLAG(0, externalMetadata,
        "[Entropix] Whether to use external metadata for local files.");

    NX_INI_FLAG(0, enableUnlimitedZoom,
        "[Entropix] Allows to zoom-in to a very big scale by removing the default zoom limitation.");

    NX_INI_FLAG(0, ignoreZoomWindowConstraints,
        "[Entropix] Whether to ignore the constrains for a zoom region.\n"
        "The default constraints are 0.1...0.9 of the linear size of the main camera window.");

    NX_INI_FLAG(0, allowCustomArZoomWindows,
        "[Entropix] Allows zoom windows to have custom aspect ratio.\n"
        "When disabled, the aspect ratio of the zoom window is the same as the aspect ratio of\n"
        "the main camera window. If enabled, allows the user to specify the aspect ratio of the\n"
        "zoom window.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms::client::desktop
