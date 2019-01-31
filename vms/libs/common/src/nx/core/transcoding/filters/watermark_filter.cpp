#include "watermark_filter.h"

#if defined(ENABLE_DATA_PROVIDERS)

#include <utils/media/frame_info.h>
#include <nx/core/watermark/watermark_images.h>

namespace nx::core::transcoding {

WatermarkImageFilter::WatermarkImageFilter(const Watermark& watermark): m_watermark(watermark)
{
}

CLVideoDecoderOutputPtr WatermarkImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    const auto frameSize = QSize(frame->width, frame->height);
    if (frameSize != m_lastFrameSize)
    {
        setImage(retrieveWatermarkImage(m_watermark, frameSize).scaled(frameSize).toImage());
        m_lastFrameSize = frameSize;
    }

    return PaintImageFilter::updateImage(frame);
}

} // namespace nx::core::transcoding

#endif // ENABLE_DATA_PROVIDERS
