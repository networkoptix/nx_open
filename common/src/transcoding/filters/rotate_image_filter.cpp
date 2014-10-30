#include "rotate_image_filter.h"
#include "utils/media/frame_info.h"

#ifdef ENABLE_DATA_PROVIDERS

QnRotateImageFilter::QnRotateImageFilter(int angle): m_angle(angle)
{
    
}

CLVideoDecoderOutputPtr QnRotateImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar)
{
    return CLVideoDecoderOutputPtr(frame->rotated(m_angle));
}

#endif // ENABLE_DATA_PROVIDERS
