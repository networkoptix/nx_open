#include "rotate_image_filter.h"
#include "utils/media/frame_info.h"

#ifdef ENABLE_DATA_PROVIDERS

QnRotateImageFilter::QnRotateImageFilter(int angle): m_angle(angle)
{
    // normalize value
    m_angle = m_angle % 360;
    if (m_angle < 0)    
        m_angle += 360;
    m_angle = int(m_angle/90.0 + 0.5)*90;
}

CLVideoDecoderOutputPtr QnRotateImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    if (m_angle == 0)
        return frame;
    else
        return CLVideoDecoderOutputPtr(frame->rotated(m_angle));
}

QSize QnRotateImageFilter::updatedResolution(const QSize& srcSize)
{
    QSize dstSize(srcSize);
    if (m_angle == 90 || m_angle == 270)
        dstSize.transpose();
    return dstSize;
}

#endif // ENABLE_DATA_PROVIDERS
