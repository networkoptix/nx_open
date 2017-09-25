#include "paint_image_filter.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <utils/media/frame_info.h>
#include <utils/color_space/yuvconvert.h>

namespace {

using AlignedBufferPtr = std::shared_ptr<quint8>;
AlignedBufferPtr createAlignedBuffer(size_t size)
{
    return AlignedBufferPtr(
        reinterpret_cast<AlignedBufferPtr::element_type*>(qMallocAligned(size, CL_MEDIA_ALIGNMENT)),
        [](AlignedBufferPtr::element_type* data)
        {
            qFreeAligned(data);
        });
}

} // namespace

class QnPaintImageFilter::Internal
{
public:
    void setImage(
        const QImage& image,
        const QPoint& position);
    void updateSourceSize(const QSize& sourceSize);


    const QImage& image() const;
    QPoint bufferOffset() const;

    QImage& targetImage();
    AlignedBufferPtr::element_type* buffer();

private:
    void updateTargetImage();

    void clearImages();

private:
    QImage m_image;
    QPoint m_initialPosition;

    QSize m_sourceSize;
    QImage m_croppedImage;

    QPoint m_bufferOffset;

    AlignedBufferPtr m_finalImageBytes;
    QImage m_finalImage;
};

void QnPaintImageFilter::Internal::setImage(
    const QImage& image,
    const QPoint& position)
{
    const auto correctedPos = QPoint(std::max(position.x(), 0), std::max(position.y(), 0));

    m_image = image;
    m_initialPosition = position;
    m_bufferOffset = QPoint(
        qPower2Floor(correctedPos.x(), CL_MEDIA_ALIGNMENT),
        qPower2Floor(correctedPos.y(), 2));

    updateTargetImage();
}

void QnPaintImageFilter::Internal::updateSourceSize(const QSize& sourceSize)
{
    if (sourceSize == m_sourceSize)
        return;

    m_sourceSize = sourceSize;
    updateTargetImage();
}

const QImage& QnPaintImageFilter::Internal::image() const
{
    return m_croppedImage;
}

QPoint QnPaintImageFilter::Internal::bufferOffset() const
{
    return m_bufferOffset;
}

QImage& QnPaintImageFilter::Internal::targetImage()
{
    return m_finalImage;
}

AlignedBufferPtr::element_type* QnPaintImageFilter::Internal::buffer()
{
    return m_finalImageBytes.get();
}

void QnPaintImageFilter::Internal::clearImages()
{
    m_croppedImage = QImage();
    m_finalImage = QImage();
    m_finalImageBytes.reset();
}

void QnPaintImageFilter::Internal::updateTargetImage()
{
    if (!m_sourceSize.isValid() || m_image.isNull())
    {
        clearImages();
        return;
    }

    const auto initialImageRect = QRect(m_initialPosition, m_image.size());
    const auto sourceRect = QRect(QPoint(0, 0), m_sourceSize);
    const auto targetImageRect = sourceRect.intersected(initialImageRect);
    const auto sourceImageRect = QRect(
        targetImageRect.topLeft() - m_initialPosition,
        targetImageRect.bottomRight() - m_initialPosition);

    if (sourceImageRect.isValid())
    {
        static const int kARGBBytesCount = 4;

        m_croppedImage = m_image.copy(sourceImageRect);

        const auto drawHeight = targetImageRect.height();
        const auto drawWidth = qPower2Ceil(
            static_cast<unsigned int>(targetImageRect.width()),
            CL_MEDIA_ALIGNMENT);

        m_finalImageBytes = createAlignedBuffer(
            drawWidth * drawHeight * kARGBBytesCount);

        const auto finalImageStride = drawWidth * kARGBBytesCount;
        m_finalImage = QImage(m_finalImageBytes.get(),
            drawWidth, drawHeight, finalImageStride, QImage::Format_ARGB32_Premultiplied);
    }
    else
    {
        clearImages();
    }
}

///

QnPaintImageFilter::QnPaintImageFilter():
    d(new Internal())
{
}

QnPaintImageFilter::~QnPaintImageFilter()
{
}

CLVideoDecoderOutputPtr QnPaintImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    d->updateSourceSize(QSize(frame->width, frame->height));

    const auto& image = d->image();
    if (image.isNull())
        return frame;

    const auto bufferOffset = d->bufferOffset();
    const int yPlaneOffset = bufferOffset.x() + bufferOffset.y() * frame->linesize[0];
    const int uvPlaneOffset = (bufferOffset.x() + bufferOffset.y() * frame->linesize[1]) / 2;

    const auto yPlane = frame->data[0] + yPlaneOffset;
    const auto uPlane = frame->data[1] + uvPlaneOffset;
    const auto vPlane = frame->data[2] + uvPlaneOffset;
    const auto yPlaneStride = frame->linesize[0];
    const auto uvPlaneStride = frame->linesize[1];

    static const auto kOpaqueAlpha = 255;

    auto& targetImage = d->targetImage();
    const auto targetWidth = targetImage.width();
    const auto targetHeight = targetImage.height();
    const auto targetStride = targetImage.bytesPerLine();

    yuv420_argb32_simd_intr(d->buffer(),
        yPlane, uPlane, vPlane,
        targetWidth, targetHeight, targetStride,
        yPlaneStride, uvPlaneStride, kOpaqueAlpha);

    QPainter painter(&targetImage);
    painter.drawImage(0, 0, image);

    static const auto kNoFlip = false;
    bgra_to_yv12_simd_intr(d->buffer(), targetStride,
        yPlane, uPlane, vPlane,
        yPlaneStride, uvPlaneStride,
        targetWidth, targetHeight, kNoFlip);

    return frame;
}

QSize QnPaintImageFilter::updatedResolution(const QSize& sourceSize)
{
    qDebug() << "---------------------" << sourceSize;
    d->updateSourceSize(sourceSize);
    return sourceSize;
}

void QnPaintImageFilter::setImage(
    const QImage& image,
    const QPoint& position)
{
    d->setImage(image, position);
}
