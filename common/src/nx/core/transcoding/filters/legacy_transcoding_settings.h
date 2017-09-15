#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <core/resource/resource_media_layout.h>
#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <transcoding/timestamp_params.h>

#include <utils/color_space/image_correction.h>

namespace nx {
namespace core {
namespace transcoding {

struct LegacyTranscodingSettings
{
    QnConstResourceVideoLayoutPtr layout;
    qreal forcedAspectRatio = 0.0;
    int rotation = 0;
    QRectF zoomWindow;
    QnMediaDewarpingParams mediaDewarpingParams;
    QnItemDewarpingParams itemDewarpingParams;
    ImageCorrectionParams contrastParams;
    QnTimeStampParams timestampParams;
    AVCodecID codec = AV_CODEC_ID_NONE;

    bool isEmpty() const
    {
        if (!qFuzzyIsNull(forcedAspectRatio))
            return false;
        if (layout && layout->channelCount() > 1)
            return false;
        if (!zoomWindow.isEmpty())
            return false;
        if (mediaDewarpingParams.enabled)
            return false;
        if (contrastParams.enabled)
            return false;
        if (rotation != 0)
            return false;
        if (timestampParams.enabled)
            return false;
        if (codec != AV_CODEC_ID_NONE)
            return false;

        return true;
    }
};

} // namespace transcoding
} // namespace core
} // namespace nx

using QnLegacyTranscodingSettings = nx::core::transcoding::LegacyTranscodingSettings;

#endif // ENABLE_DATA_PROVIDERS
