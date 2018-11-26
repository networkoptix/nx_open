#pragma once
#if defined(ENABLE_DATA_PROVIDERS)

#include <memory>

#include <QtGui/QImage>

#include <transcoding/filters/abstract_image_filter.h>

class QImage;

namespace nx {
namespace core {
namespace transcoding {
namespace detail {

class ImageToFramePainter
{
public:
    ImageToFramePainter();
    ~ImageToFramePainter();

    void setImage(
        const QImage& image,
        const QPoint& offset,
        Qt::Alignment alignment);

    void updateSourceSize(const QSize& sourceSize);

    CLVideoDecoderOutputPtr drawTo(const CLVideoDecoderOutputPtr& frame);

private:
    void updateTargetImage();

    void clearImages();

private:
    using AlignedBufferPtr = std::shared_ptr<quint8>;

    QImage m_image;
    QPoint m_offset;
    Qt::Alignment m_alignment;

    QSize m_sourceSize;
    QImage m_croppedImage;

    QPoint m_bufferOffset;
    QPoint m_imageOffsetInBuffer;

    AlignedBufferPtr m_finalImageBytes;
    QImage m_finalImage;
};

} // namespace detail
} // namespace transcoding
} // namespace core
} // namespace nx

#endif // defined(ENABLE_DATA_PROVIDERS)
