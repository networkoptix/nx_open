#include "paint_image_filter.h"

#include <transcoding/filters/image_to_frame_painter.h>

namespace nx {
namespace transcoding {
namespace filters {

PaintImageFilter::PaintImageFilter():
    m_painter(new detail::ImageToFramePainter())
{
}

PaintImageFilter::~PaintImageFilter()
{
}

CLVideoDecoderOutputPtr PaintImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    return m_painter->drawTo(frame);
}

QSize PaintImageFilter::updatedResolution(const QSize& sourceSize)
{
    m_painter->updateSourceSize(sourceSize);
    return sourceSize;
}

void PaintImageFilter::setImage(
    const QImage& image,
    const QPoint& offset,
    Qt::Alignment alignment)
{
    m_painter->setImage(image, offset, alignment);
}

} // namespace filters
} // namespace transcoding
} // namespace nx
