#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_media_stream_info.h>

namespace nx { namespace media { struct CameraStreamCapability; } }

namespace nx {
namespace core {

struct CameraBitrateCalculator
{
    static float suggestBitrateForQualityKbps(
        Qn::StreamQuality quality,
        QSize resolution,
        int fps);

    static float suggestBitrateForQualityKbps(
        Qn::StreamQuality quality,
        QSize resolution,
        int fps,
        media::CameraStreamCapability streamCapability,
        Qn::BitratePerGopType bitratePerGopType);

    static float roundKbpsToMbps(float kbps, int decimals = 1);
    static float roundMbpsToKbps(float mbps, int decimals = 1);

    // TODO: #GDM Remove this overload when old camera settings will be removed.
    static float getBitrateForQualityMbps(
        const QnVirtualCameraResourcePtr& camera,
        Qn::StreamQuality quality,
        int fps,
        int decimals = 1);
};

} // namespace core
} // namespace nx
