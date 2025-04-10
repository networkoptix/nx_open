// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "media_settings.h"

namespace nx::vms::api {

/**%apidoc
 * Media delivery method.
 */
NX_REFLECTION_ENUM_CLASS(WebRtcMethod,
    srtp, /**<%apidoc Use Secure RTP (a standard way). */
    mse /**<%apidoc Use media chunks via Data Channel (a non-standard way). */
);

struct NX_VMS_API WebRtcTrackerSettings: public MediaSettings
{
    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * /rest/v{1-}/devices) or MAC address (not supported for certain Devices).
     */
    nx::Uuid id;

    /**%apidoc
     * If specified, regulates the speed of streaming from the Archive. Has no effect for live
     * streaming.
     *
     * %value 1.0 Default value.
     * %value unlimited Stream with the maximum possible speed.
     */
    std::optional<std::string> speed{"1.0"};

    /**%apidoc[opt] */
    WebRtcMethod deliveryMethod = WebRtcMethod::srtp;

    /**%apidoc[opt] If specified, don't resend lost UDP packets. */
    bool unreliableTransport = false;

    /**%apidoc
     * Media format for MSE delivery method.
     */
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(MseFormat,
        mpegts,
        mp4
    );

    /**%apidoc[opt] MSE delivery only, media format(container) in which media data will be mixed.
     */
    MseFormat mseFormat = MseFormat::mp4;

    /**%apidoc[opt]
     * Specifies the interval at which the timestamp is sent. If it is zero, it will be sent every
     * frame.
     */
    std::chrono::milliseconds sendTimestampIntervalMs{1000};
};
#define WebRtcTrackerSettings_Fields MediaSettings_Fields(id)(speed)(deliveryMethod)(unreliableTransport)(mseFormat)(sendTimestampIntervalMs)
QN_FUSION_DECLARE_FUNCTIONS(WebRtcTrackerSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(WebRtcTrackerSettings, WebRtcTrackerSettings_Fields)

} // namespace nx::vms::api
