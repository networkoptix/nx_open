#include "watermark_filter.h"

#if defined(ENABLE_DATA_PROVIDERS)

#include <nx/core/watermark/watermark_images.h>

namespace nx {
namespace core {
namespace transcoding {

WatermarkImageFilter::WatermarkImageFilter(const Watermark& watermark): m_watermark(watermark)
{
}

QSize WatermarkImageFilter::updatedResolution(const QSize& sourceSize)
{
    PaintImageFilter::updatedResolution(sourceSize);
    setImage(createWatermarkImage(m_watermark, sourceSize).toImage());
    return sourceSize;
}

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
