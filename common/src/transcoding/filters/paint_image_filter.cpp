#include "paint_image_filter.h"

#include <QtGui/QImage>

#include <utils/media/frame_info.h>

namespace {

using AlignedBufferPtr = std::shared_ptr<quint8>;
AlignedBufferPtr createAlignedBuffer(size_t size)
{
    return AlignedBufferPtr(qMallocAligned(size, CL_MEDIA_ALIGNMENT),
        [](void* data)
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
    const QPoint& pos() const;

private:
    void updateTargetImage();


private:
    QImage m_image;
    QPoint m_position;
    QRect m_imageRect;
    QPoint m_targetPos;

    QSize m_sourceSize;
    QImage m_croppedImage;

    AlignedBufferPtr m_imageBuffer;
};

void QnPaintImageFilter::Internal::setImage(
    const QImage& image,
    const QPoint& position)
{
    m_image = image;
    m_position = position;
    m_imageRect = QRect(position, image.size());
    m_targetPos = QPoint(std::max(position.x(), 0), std::max(position.y(), 0));

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

const QPoint& QnPaintImageFilter::Internal::pos() const
{
    return m_targetPos;
}

void QnPaintImageFilter::Internal::updateTargetImage()
{
    if (!m_sourceSize.isValid() || m_image.isNull())
    {
        m_croppedImage = QImage();
        return;
    }

    const auto sourceRect = QRect(QPoint(0, 0), m_sourceSize);
    const auto targetImageRect = sourceRect.intersected(m_imageRect);
    const auto sourceImageRect = QRect(
        targetImageRect.topLeft() - m_position,
        targetImageRect.bottomRight() - m_position);

    m_croppedImage = sourceImageRect.isValid()
        ? m_image.copy(sourceImageRect)
        : QImage();
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



    return frame;
}

QSize QnPaintImageFilter::updatedResolution(const QSize& sourceSize)
{
    d->updateSourceSize(sourceSize);
    return sourceSize;
}

void QnPaintImageFilter::setImage(
    const QImage& image,
    const QPoint& position)
{
    d->setImage(image, position);
}
