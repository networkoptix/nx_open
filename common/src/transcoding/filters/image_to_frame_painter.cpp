#include "image_to_frame_painter.h"

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

namespace nx {
namespace transcoding {
namespace filters {
namespace detail {

ImageToFramePainter::ImageToFramePainter()
{
}

ImageToFramePainter::~ImageToFramePainter()
{
}

void ImageToFramePainter::setImage(
    const QImage& image,
    const QPoint& position,
    Qt::Alignment alignment)
{
    const auto correctedPos = QPoint(std::max(position.x(), 0), std::max(position.y(), 0));

    m_image = image;
    m_initialPosition = position;
    m_bufferOffset = QPoint(
        qPower2Floor(correctedPos.x(), CL_MEDIA_ALIGNMENT),
        qPower2Floor(correctedPos.y(), 2));

    updateTargetImage();
}

void ImageToFramePainter::updateSourceSize(const QSize& sourceSize)
{
    if (sourceSize == m_sourceSize)
        return;

    m_sourceSize = sourceSize;
    updateTargetImage();
}

CLVideoDecoderOutputPtr ImageToFramePainter::drawTo(const CLVideoDecoderOutputPtr& frame)
{
    updateSourceSize(QSize(frame->width, frame->height));

    if (m_croppedImage.isNull())
        return frame;

    const int yPlaneOffset = m_bufferOffset.x() + m_bufferOffset.y() * frame->linesize[0];
    const int uvPlaneOffset = (m_bufferOffset.x() + m_bufferOffset.y() * frame->linesize[1]) / 2;

    const auto yPlane = frame->data[0] + yPlaneOffset;
    const auto uPlane = frame->data[1] + uvPlaneOffset;
    const auto vPlane = frame->data[2] + uvPlaneOffset;
    const auto yPlaneStride = frame->linesize[0];
    const auto uvPlaneStride = frame->linesize[1];

    static const auto kOpaqueAlpha = 255;

    const auto targetWidth = m_finalImage.width();
    const auto targetHeight = m_finalImage.height();
    const auto targetStride = m_finalImage.bytesPerLine();

    yuv420_argb32_simd_intr(m_finalImageBytes.get(),
        yPlane, uPlane, vPlane,
        targetWidth, targetHeight, targetStride,
        yPlaneStride, uvPlaneStride, kOpaqueAlpha);

    QPainter painter(&m_finalImage);
    painter.drawImage(0, 0, m_croppedImage);

    static const auto kNoFlip = false;
    bgra_to_yv12_simd_intr(m_finalImageBytes.get(), targetStride,
        yPlane, uPlane, vPlane,
        yPlaneStride, uvPlaneStride,
        targetWidth, targetHeight, kNoFlip);

    return frame;
}


void ImageToFramePainter::clearImages()
{
    m_croppedImage = QImage();
    m_finalImage = QImage();
    m_finalImageBytes.reset();
}

void ImageToFramePainter::updateTargetImage()
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

} // namespace detail
} // namespace filters
} // namespace transcoding
} // namespace nx
