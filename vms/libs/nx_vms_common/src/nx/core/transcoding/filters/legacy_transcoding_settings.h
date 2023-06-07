// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

#include <core/resource/resource_fwd.h>
#include <nx/core/transcoding/filters/timestamp_params.h>
#include <nx/core/watermark/watermark.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <nx/vms/api/data/media_settings.h>
#include <utils/common/aspect_ratio.h>

namespace nx {
namespace core {
namespace transcoding {

struct NX_VMS_COMMON_API LegacyTranscodingSettings
{
    LegacyTranscodingSettings() = default;
    LegacyTranscodingSettings(
        const QnMediaResourcePtr& resource,
        const nx::vms::api::MediaSettings* settings = nullptr);
    LegacyTranscodingSettings(const LegacyTranscodingSettings&) = default;
    LegacyTranscodingSettings& operator=(const LegacyTranscodingSettings&) = default;

    QnMediaResourcePtr resource;

    QnAspectRatio forcedAspectRatio;
    int rotation = 0;
    QRectF zoomWindow;
    nx::vms::api::ImageCorrectionData contrastParams;
    bool forceDewarping = false;
    nx::vms::api::dewarping::ViewData itemDewarpingParams;
    nx::core::transcoding::TimestampParams timestampParams;
    nx::core::transcoding::FilterParams cameraNameParams;
    Watermark watermark;
    bool panoramic = false;

    bool isEmpty() const;
};

} // namespace transcoding
} // namespace core
} // namespace nx

using QnLegacyTranscodingSettings = nx::core::transcoding::LegacyTranscodingSettings;
