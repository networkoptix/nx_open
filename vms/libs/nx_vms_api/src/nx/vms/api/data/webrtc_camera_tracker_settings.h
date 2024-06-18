// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "media_settings.h"

namespace nx::vms::api {

struct NX_VMS_API WebRtcCameraTrackerSettings
{
    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * /rest/v{1-}/devices) or MAC address (not supported for certain Devices).
     */
    nx::Uuid id;
};

#define WebRtcCameraTrackerSettings_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(WebRtcCameraTrackerSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(WebRtcCameraTrackerSettings, WebRtcCameraTrackerSettings_Fields)

} // namespace nx::vms::api
