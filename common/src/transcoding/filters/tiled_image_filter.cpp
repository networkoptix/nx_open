#include "tiled_image_filter.h"
#include "core/resource/resource_media_layout.h"
#include "utils/media/frame_info.h"
#include "crop_image_filter.h"

#ifdef ENABLE_DATA_PROVIDERS

QnTiledImageFilter::QnTiledImageFilter(QSharedPointer<const QnResourceVideoLayout> videoLayout): m_layout(videoLayout)
{

}

CLVideoDecoderOutputPtr QnTiledImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar)
{
    if (m_layout->size().width() == 1 && m_layout->size().height() == 1)
        return frame;

    QSize size(frame->width, frame->height);
    if (!m_tiledFrame || size != m_size) {
        size = m_size;
        m_tiledFrame = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput());
        m_tiledFrame->reallocate(frame->width * m_layout->size().width(), frame->height * m_layout->size().height(), frame->format);
        m_tiledFrame->memZerro();
    }

    QPoint pos = m_layout->position(frame->channel);
    QRect rect(pos.x() * m_size.width(), pos.x() * m_size.height(), m_size.width(), m_size.height());
    CLVideoDecoderOutputPtr croppedFrame = QnCropImageFilter(rect).updateImage(m_tiledFrame, updateRect, ar);
    CLVideoDecoderOutput::copy(frame.data(), croppedFrame.data());
    return m_tiledFrame;
}

QSize QnTiledImageFilter::updatedResolution(const QSize& srcSize)
{
    return QSize(srcSize.width() * m_layout->size().width(), srcSize.height() * m_layout->size().height());
}

#endif // ENABLE_DATA_PROVIDERS
