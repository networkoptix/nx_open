// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

#include <core/resource/resource_fwd.h>

#include <nx/core/watermark/watermark.h>

#include <utils/common/aspect_ratio.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

#include <nx/core/transcoding/filters/timestamp_params.h>

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
    nx::vms::api::dewarping::ViewData itemDewarpingParams;
    nx::core::transcoding::TimestampParams timestampParams;
    nx::core::transcoding::FilterParams cameraNameParams;
    Watermark watermark;

    bool isEmpty() const;
};

} // namespace transcoding
} // namespace core
} // namespace nx

using QnLegacyTranscodingSettings = nx::core::transcoding::LegacyTranscodingSettings;
