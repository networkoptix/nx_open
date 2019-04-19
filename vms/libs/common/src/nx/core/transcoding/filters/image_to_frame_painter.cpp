#include "image_to_frame_painter.h"
#if defined(ENABLE_DATA_PROVIDERS)

#include <QtGui/QPainter>

#include <utils/media/frame_info.h>
#include <utils/color_space/yuvconvert.h>
#include "utils/common/app_info.h"
#include <nx/utils/log/log_main.h>

extern "C" {
#include <libswscale/swscale.h>
} // extern "C"

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

QPoint calculateFinalPosition(
    const QPoint& offset,
    Qt::Alignment alignment,
    const QSize& imageSize,
    const QSize& frameSize)
{
    QPoint result = offset;
    if (alignment.testFlag(Qt::AlignAbsolute))
        return result;

    if (alignment.testFlag(Qt::AlignHCenter))
        result.setX((frameSize.width() - imageSize.width())/ 2 + offset.x());
    else if (alignment.testFlag(Qt::AlignRight) || alignment.testFlag(Qt::AlignTrailing))
        result.setX(frameSize.width() - offset.x() - imageSize.width());
    else
        result.setX(offset.x()); //< Set left offset by default.

    if (alignment.testFlag(Qt::AlignVCenter))
        result.setY((frameSize.height() - imageSize.height()) / 2 + offset.y());
    else if (alignment.testFlag(Qt::AlignBottom))
        result.setY(frameSize.height() - offset.y() - imageSize.height());
    else
        result.setY(offset.y()); //< Set top offset by default

    return result;
}

} // namespace

namespace nx {
namespace core {
namespace transcoding {
namespace detail {

ImageToFramePainter::ImageToFramePainter()
{
}

ImageToFramePainter::~ImageToFramePainter()
{
    sws_freeContext(m_toImageContext);
    sws_freeContext(m_fromImageContext);
}

void ImageToFramePainter::setImage(
    const QImage& image,
    const QPoint& offset,
    Qt::Alignment alignment)
{
    m_image = image;

    m_alignment = alignment;
    m_offset = offset;

    updateTargetImage();
}

void ImageToFramePainter::updateTargetImage(const QSize& sourceSize, AVPixelFormat pixelFormat)
{
    if (sourceSize == m_sourceSize && m_pixelFormat == pixelFormat)
        return;

    m_sourceSize = sourceSize;
    m_pixelFormat = pixelFormat;
    updateTargetImage();
}

CLVideoDecoderOutputPtr ImageToFramePainter::drawToFfmpeg(const CLVideoDecoderOutputPtr& frame)
{
    const int yPlaneOffset = m_bufferOffset.x() + m_bufferOffset.y() * frame->linesize[0];
    const int uvPlaneOffset = (m_bufferOffset.x() + m_bufferOffset.y() * frame->linesize[1]) / 2;

    if (!m_toImageContext)
    {
        m_toImageContext = sws_getContext(
            m_finalImage.width(), m_finalImage.height(), (AVPixelFormat)frame->format,
            m_finalImage.width(), m_finalImage.height(), AV_PIX_FMT_BGRA,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
    }
    if (!m_fromImageContext)
    {
        m_fromImageContext = sws_getContext(
            m_finalImage.width(), m_finalImage.height(), AV_PIX_FMT_BGRA,
            m_finalImage.width(), m_finalImage.height(), (AVPixelFormat)frame->format,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    if (!m_toImageContext || !m_fromImageContext)
    {
        NX_WARNING(this, "Can't allocate sws scale context for color conversion");
        return frame;
    }

    quint8* frameData[4] = {
        frame->data[0] + yPlaneOffset,
        frame->data[1] + uvPlaneOffset,
        frame->data[2] + uvPlaneOffset,
        nullptr
    };

    quint8* dstData[4] = { (quint8*)m_finalImage.bits(), nullptr, nullptr, nullptr };
    int dstStride[4] = { m_finalImage.bytesPerLine(), 0, 0, 0 };
    sws_scale(
        m_toImageContext, frameData, frame->linesize,
        0, m_finalImage.height(),
        dstData, dstStride);

    QPainter painter(&m_finalImage);
    painter.drawImage(m_imageOffsetInBuffer, m_croppedImage);
    painter.end();

    sws_scale(
        m_fromImageContext, dstData, dstStride,
        0, m_finalImage.height(),
        frameData, frame->linesize);

    return frame;
}

CLVideoDecoderOutputPtr ImageToFramePainter::drawToSse(const CLVideoDecoderOutputPtr& frame)
{
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
    painter.drawImage(m_imageOffsetInBuffer, m_croppedImage);
    painter.end();

    bgra_to_yv12_simd_intr(m_finalImageBytes.get(), targetStride,
        yPlane, uPlane, vPlane,
        yPlaneStride, uvPlaneStride,
        targetWidth, targetHeight, false /*don't flip*/);

    return frame;
}

CLVideoDecoderOutputPtr ImageToFramePainter::drawTo(const CLVideoDecoderOutputPtr& frame)
{
    updateTargetImage(QSize(frame->width, frame->height), (AVPixelFormat) frame->format);

    if (m_croppedImage.isNull())
        return frame;

    if (frame->format != AV_PIX_FMT_YUV420P || QnAppInfo::isArm())
        return drawToFfmpeg(frame);
    else
        return drawToSse(frame);
}

void ImageToFramePainter::clearImages()
{
    m_croppedImage = QImage();
    m_finalImage = QImage();
    m_finalImageBytes.reset();
}

void ImageToFramePainter::updateTargetImage()
{
    sws_freeContext(m_toImageContext);
    sws_freeContext(m_fromImageContext);
    m_toImageContext = nullptr;
    m_fromImageContext = nullptr;

    if (!m_sourceSize.isValid() || m_image.isNull())
    {
        clearImages();
        return;
    }

    const auto finalPosition = calculateFinalPosition(
        m_offset, m_alignment, m_image.size(), m_sourceSize);

    const auto correctedPos = QPoint(
        std::max(finalPosition.x(), 0), std::max(finalPosition.y(), 0));
    m_bufferOffset = QPoint(
        qPower2Floor(correctedPos.x(), CL_MEDIA_ALIGNMENT),
        qPower2Floor(correctedPos.y(), 2));
    m_imageOffsetInBuffer = finalPosition - m_bufferOffset;

    const auto initialImageRect = QRect(finalPosition, m_image.size());
    const auto sourceRect = QRect(QPoint(0, 0), m_sourceSize);
    const auto targetImageRect = sourceRect.intersected(initialImageRect);
    const auto sourceImageRect = QRect(
        targetImageRect.topLeft() - finalPosition,
        targetImageRect.bottomRight() - finalPosition);

    if (sourceImageRect.isValid())
    {
        static const int kARGBBytesCount = 4;

        m_croppedImage = m_image.copy(sourceImageRect);

        const auto drawHeight = qPower2Ceil(
            static_cast<unsigned int>(targetImageRect.height()), 2);
        const auto drawWidth = qPower2Ceil(
            static_cast<unsigned int>(targetImageRect.width()), CL_MEDIA_ALIGNMENT);

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
} // namespace transcoding
} // namespace core
} // namespace nx

#endif // defined(ENABLE_DATA_PROVIDERS)
