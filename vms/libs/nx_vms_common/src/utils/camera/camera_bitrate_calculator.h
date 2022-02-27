// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_media_stream_info.h>

#include <common/common_globals.h>

namespace nx { namespace media { struct CameraStreamCapability; } }

namespace nx::vms::common {

struct NX_VMS_COMMON_API CameraBitrateCalculator
{
    static float suggestBitrateForQualityKbps(
        Qn::StreamQuality quality,
        QSize resolution,
        int fps,
        const QString& codec);

    static float suggestBitrateForQualityKbps(
        Qn::StreamQuality quality,
        QSize resolution,
        int fps,
        const QString& codec,
        media::CameraStreamCapability streamCapability,
        bool useBitratePerGop);

    static float roundKbpsToMbps(float kbps, int decimals = 1);
    static float roundMbpsToKbps(float mbps, int decimals = 1);

    static constexpr int kBitrateKbpsPrecisionDecimals = 1;
};

} // namespace nx::vms::common
