#pragma once

#include <nx/vms/api/analytics/stream_type.h>

extern "C" {

#include <libavutil/pixfmt.h>

} // extern "C"

namespace nx::vms::server::analytics {

struct StreamRequirements
{
    nx::vms::api::analytics::StreamTypes requiredStreamTypes;
    AVPixelFormat uncompressedFramePixelFormat = AV_PIX_FMT_NONE;
    nx::vms::api::StreamIndex preferredStreamIndex = nx::vms::api::StreamIndex::undefined;
};

enum class MotionAnalysisPolicy
{
    // Motion should be analyzed if video stream complies with certain requirements (resolution,
    // stream index, certain camera capabilities).
    automatic,

    // Motion must not be analyzed even if stream is compliant with motion analysis requirements.
    disabled,

    // Motion have to be analyzed even if video stream doesn't comply with motion analysis
    // requirements.
    forced,
};

struct StreamProviderRequirements
{
    nx::vms::api::analytics::StreamTypes requiredStreamTypes;
    MotionAnalysisPolicy motionAnalysisPolicy = MotionAnalysisPolicy::automatic;

};

} // namespace nx::vms::server::analytics
