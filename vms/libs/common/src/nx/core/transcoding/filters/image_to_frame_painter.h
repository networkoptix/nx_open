#pragma once
#if defined(ENABLE_DATA_PROVIDERS)

#include <memory>

#include <QtGui/QImage>

#include <transcoding/filters/abstract_image_filter.h>

extern "C"
{
#include "libavutil/pixfmt.h"
} // extern "C"

class QImage;
struct SwsContext;

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

    CLVideoDecoderOutputPtr drawTo(const CLVideoDecoderOutputPtr& frame);

private:
    void updateTargetImage(const QSize& sourceSize, AVPixelFormat format);
    void updateTargetImage();

    void clearImages();
    CLVideoDecoderOutputPtr drawToFfmpeg(const CLVideoDecoderOutputPtr& frame);
    CLVideoDecoderOutputPtr drawToSse(const CLVideoDecoderOutputPtr& frame);
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

    SwsContext* m_toImageContext = nullptr;
    SwsContext* m_fromImageContext = nullptr;
    AVPixelFormat m_pixelFormat = AV_PIX_FMT_NONE;
};

} // namespace detail
} // namespace transcoding
} // namespace core
} // namespace nx

#endif // defined(ENABLE_DATA_PROVIDERS)
