// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::api {

struct NX_VMS_API WebRtcTrackerSettings
{
    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * /rest/v{1-}/devices) or MAC address (not supported for certain Devices).
     */
    QnUuid id;

    /**%apidoc
     * If present, specifies the Archive stream start position. Otherwise, the Live stream is
     * provided.
     */
    std::optional<std::chrono::microseconds> positionUs;

    /**%apidoc
     * If specified, regulates the speed of streaming from the Archive. Has no effect for live
     * streaming.
     * 
     * %value 1.0 Default value.
     * %value unlimited Stream with the maximum possible speed.
     */
    std::optional<std::string> speed{"1.0"};

    StreamIndex stream = nx::vms::api::StreamIndex::undefined;
};
#define WebRtcTrackerSettings_Fields (id)(positionUs)(speed)(stream)
QN_FUSION_DECLARE_FUNCTIONS(WebRtcTrackerSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(WebRtcTrackerSettings, WebRtcTrackerSettings_Fields)

} // namespace nx::vms::api

