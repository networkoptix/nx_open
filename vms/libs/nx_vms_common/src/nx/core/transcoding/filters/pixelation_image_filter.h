// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/pixelation_settings.h>

#include "paint_image_filter.h"

namespace nx::vms::common { class PixelationSettings; }
namespace nx::vms::common::pixelation { class Pixelation; }

namespace nx::core::transcoding {

class NX_VMS_COMMON_API PixelationImageFilter: public PaintImageFilter
{
public:
    PixelationImageFilter(const nx::vms::api::PixelationSettings& settings);
    ~PixelationImageFilter();

    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) override;

    virtual QSize updatedResolution(const QSize& sourceSize) { return sourceSize; };

private:
    std::shared_ptr<nx::vms::common::pixelation::Pixelation> m_pixelation;
    nx::vms::api::PixelationSettings m_settings;
};

} // nx::core::transcoding
