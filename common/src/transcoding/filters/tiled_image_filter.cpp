#include "tiled_image_filter.h"
#include "core/resource/resource_media_layout.h"
#include "utils/media/frame_info.h"
#include "crop_image_filter.h"
#include "utils/common/util.h"

#ifdef ENABLE_DATA_PROVIDERS

QnTiledImageFilter::QnTiledImageFilter(const QSharedPointer<const QnResourceVideoLayout>& videoLayout): m_layout(videoLayout), m_prevFrameTime(0)
{

}

CLVideoDecoderOutputPtr QnTiledImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    if (m_layout->size().width() == 1 && m_layout->size().height() == 1)
        return frame;

    QSize size(frame->width, frame->height);
    if (!m_tiledFrame || size != m_size) {
        m_size = size;
        QSize newSize(frame->width * m_layout->size().width(), frame->height * m_layout->size().height());
        if (m_tiledFrame)
            m_tiledFrame = CLVideoDecoderOutputPtr(m_tiledFrame->scaled(newSize, (AVPixelFormat) frame->format));
        else {
            m_tiledFrame = CLVideoDecoderOutputPtr(new CLVideoDecoderOutput());
            m_tiledFrame->reallocate(newSize, frame->format);
            m_tiledFrame->memZerro();
        }
    }
    m_tiledFrame->assignMiscData(frame.data());
    m_prevFrameTime = m_tiledFrame->pts = qMax( static_cast<qint64>(m_tiledFrame->pts) , m_prevFrameTime + MIN_FRAME_DURATION );

    QPoint pos = m_layout->position(frame->channel);
    QRect rect(pos.x() * m_size.width(), pos.y() * m_size.height(), m_size.width(), m_size.height());
    CLVideoDecoderOutputPtr croppedFrame = QnCropImageFilter(rect).updateImage(m_tiledFrame);
    CLVideoDecoderOutput::copy(frame.data(), croppedFrame.data());
    return m_tiledFrame;
}

QSize QnTiledImageFilter::updatedResolution(const QSize& srcSize)
{
    return QSize(srcSize.width() * m_layout->size().width(), srcSize.height() * m_layout->size().height());
}

#endif // ENABLE_DATA_PROVIDERS
