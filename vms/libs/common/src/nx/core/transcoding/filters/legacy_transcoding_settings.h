#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <QtCore/QRect>

#include <core/resource/resource_fwd.h>

#include <transcoding/timestamp_params.h>

#include <nx/core/watermark/watermark.h>

#include <utils/common/aspect_ratio.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

namespace nx {
namespace core {
namespace transcoding {

struct LegacyTranscodingSettings
{
    QnMediaResourcePtr resource;

    QnAspectRatio forcedAspectRatio;
    int rotation = 0;
    QRectF zoomWindow;
    nx::vms::api::ImageCorrectionData contrastParams;
    nx::vms::api::DewarpingData itemDewarpingParams;
    QnTimeStampParams timestampParams;
    Watermark watermark;

    bool isEmpty() const;
};

} // namespace transcoding
} // namespace core
} // namespace nx

using QnLegacyTranscodingSettings = nx::core::transcoding::LegacyTranscodingSettings;

#endif // ENABLE_DATA_PROVIDERS
