// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_client_core.ini") { reload(); }

    // VMS-39456.
    NX_INI_FLAG(false, useNativeServerTimeZone,
        "[Support] Use OS-provided timezone when displaying UI elements based on server time.");

    // VMS-30347.
    NX_INI_INT(8, maxLastConnectedTilesStored,
        "[Support] Maximum last connected systems tiles stored on the Welcome Screen");

    NX_INI_FLAG(false, doNotPingCloudSystems,
        "[Dev] Disable cloud system pings, so desktop client will rely on cloud status only");

    NX_INI_STRING("", dumpGeneratedIconsTo,
        "[Dev] Dump icons, generated from svg, to a given folder.");

    NX_INI_INT(0, systemsHideOptions,
        "[Dev] Hide systems, bitwise combination of flags:\n"
        " * 1 - Incompatible systems.\n"
        " * 2 - Not connectable cloud systems.\n"
        " * 4 - Compatible systems which require compatibility mode.\n"
        " * 8 - All systems except those on localhost.\n"
    );

    NX_INI_INT(30000, cameraDataLoadingIntervalMs,
        "[Dev] Interval, at which the Client loads camera data chunks.");

    NX_INI_STRING("", rootCertificatesFolder,
        "Path to a folder with user-provided certificates. If set, all *.pem and *.crt files\n"
        "from this folder are used as trusted root certificates together with the system ones.");

    NX_INI_FLAG(false, delayLiveAnalyticsData,
        "[Support] Prohibits showing live analytics before the corresponding frame appears on\n"
        "the camera if the camera is playing live");

    NX_INI_FLAG(false, showDebugTimeInformationInEventSearchData,
        "[Dev] Whether to show extra timestamp information in the event search data.");

    NX_INI_FLOAT(1.0f, attributeTableLineHeightFactor,
        "[Design] Line height multiplier for analytics attribute tables.");

    NX_INI_INT(4, attributeTableSpacing,
        "[Design] Spacing between attributes in analytics attribute tables.");

    NX_INI_INT(1000, intervalPreviewDelayMs,
        "[Design] The delay time in milliseconds before interval video playback is initiated\n"
        "after a Search Panel thumbnail is hovered.");

    NX_INI_INT(500, intervalPreviewLoopDelayMs,
        "[Design] The delay time in milliseconds before interval video playback is restarted.");

    NX_INI_INT(8000, intervalPreviewDurationMs,
        "[Design] The duration of interval video playback in milliseconds.\n"
        "Half of it is before exact preview timestamp and another half is after.");

    NX_INI_INT(4, intervalPreviewSpeedFactor,
        "[Design] Interval preview video playback speed multiplier.");

    NX_INI_FLAG(false, delayRightPanelLiveAnalytics,
        "[Support] Prohibits showing right panel live analytics before the corresponding frame\n"
        "appears on the camera if the camera is playing live");
};

NX_VMS_CLIENT_CORE_API Ini& ini();

} // namespace nx::vms::client::core
