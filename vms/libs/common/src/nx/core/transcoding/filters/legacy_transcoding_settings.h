#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <core/resource/resource_fwd.h>

#include <core/ptz/item_dewarping_params.h>
#include <transcoding/timestamp_params.h>

#include <nx/core/watermark/watermark.h>

#include <utils/common/aspect_ratio.h>
#include <utils/color_space/image_correction.h>

namespace nx {
namespace core {
namespace transcoding {

struct LegacyTranscodingSettings
{
    QnMediaResourcePtr resource;

    QnAspectRatio forcedAspectRatio;
    int rotation = 0;
    QRectF zoomWindow;
    QnItemDewarpingParams itemDewarpingParams;
    ImageCorrectionParams contrastParams;
    QnTimeStampParams timestampParams;
    Watermark watermark;

    bool isEmpty() const;
};

} // namespace transcoding
} // namespace core
} // namespace nx

using QnLegacyTranscodingSettings = nx::core::transcoding::LegacyTranscodingSettings;

#endif // ENABLE_DATA_PROVIDERS
