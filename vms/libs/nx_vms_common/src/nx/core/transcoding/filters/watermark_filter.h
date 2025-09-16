// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/core/watermark/watermark.h>

#include "paint_image_filter.h"

namespace nx::core::transcoding {

struct WatermarkOverlaySettings;

class WatermarkImageFilter: public PaintImageFilter
{
public:
    WatermarkImageFilter(const Watermark& watermark);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;

private:
    Watermark m_watermark;
    QSize m_lastFrameSize;
};

} // namespace nx::core::transcoding
