#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

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

#endif // ENABLE_DATA_PROVIDERS
