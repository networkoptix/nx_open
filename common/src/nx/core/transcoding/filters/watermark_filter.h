#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <nx/core/watermark/watermark.h>

#include "paint_image_filter.h"

namespace nx {
namespace core {
namespace transcoding {

struct WatermarkOverlaySettings;

class WatermarkImageFilter: public PaintImageFilter
{
public:
    WatermarkImageFilter(const Watermark& watermark);

    virtual QSize updatedResolution(const QSize& sourceSize) override;

private:
  Watermark m_watermark;
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
