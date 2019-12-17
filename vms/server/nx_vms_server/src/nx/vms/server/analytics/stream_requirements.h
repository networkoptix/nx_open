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

} // namespace nx::vms::server::analytics
