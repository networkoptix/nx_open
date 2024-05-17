// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "paint_image_filter.h"

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

CLVideoDecoderOutputPtr PaintImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame,
    const QnAbstractCompressedMetadataPtr&)
{
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
