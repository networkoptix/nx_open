#include "paint_image_filter.h"

#if defined(ENABLE_DATA_PROVIDERS)

#include <utils/media/frame_info.h>
#include <nx/core/transcoding/filters/image_to_frame_painter.h>

namespace nx {
namespace core {
namespace transcoding {

PaintImageFilter::PaintImageFilter():
    m_painter(new detail::ImageToFramePainter())
{
}

PaintImageFilter::~PaintImageFilter()
{
}

CLVideoDecoderOutputPtr PaintImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    m_painter->updateSourceSize(QSize(frame->width, frame->height)); //< Does nothing if no size change.
    return m_painter->drawTo(frame);
}

QSize PaintImageFilter::updatedResolution(const QSize& sourceSize)
{
    return sourceSize;
}

void PaintImageFilter::setImage(
    const QImage& image,
    const QPoint& offset,
    Qt::Alignment alignment)
{
    m_painter->setImage(image, offset, alignment);
}

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
