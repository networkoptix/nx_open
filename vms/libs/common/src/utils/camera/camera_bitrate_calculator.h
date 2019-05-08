#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_media_stream_info.h>

#include <common/common_globals.h>

namespace nx { namespace media { struct CameraStreamCapability; } }

namespace nx {
namespace core {

struct CameraBitrateCalculator
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

    // TODO: #GDM Remove this overload when old camera settings is removed
    //    and schedule export is refactored.
    static float getBitrateForQualityMbps(
        const QnVirtualCameraResourcePtr& camera,
        Qn::StreamQuality quality,
        int fps,
        const QString& codec);

    static constexpr int kBitrateKbpsPrecisionDecimals = 1;
};

} // namespace core
} // namespace nx
