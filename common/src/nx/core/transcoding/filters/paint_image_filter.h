
#pragma once

#include <transcoding/filters/abstract_image_filter.h>

namespace nx {
namespace core {
namespace transcoding {

namespace detail {

class ImageToFramePainter;

} // namespace detail

class PaintImageFilter: public QnAbstractImageFilter
{
public:
    PaintImageFilter();
    virtual ~PaintImageFilter();

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& sourceSize) override;

    void setImage(
        const QImage& image,
        const QPoint& offset,
        Qt::Alignment alignment);

private:
    const QScopedPointer<detail::ImageToFramePainter> m_painter;
};

} // namespace transcoding
} // namespace core
} // namespace nx
