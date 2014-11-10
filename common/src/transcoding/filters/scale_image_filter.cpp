#include "scale_image_filter.h"
#include "utils/media/frame_info.h"

#ifdef ENABLE_DATA_PROVIDERS

QnScaleImageFilter::QnScaleImageFilter(const QSize& size, PixelFormat format): m_size(size), m_format(format)
{
}

CLVideoDecoderOutputPtr QnScaleImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    return CLVideoDecoderOutputPtr(frame->scaled(m_size, m_format));
}

QSize QnScaleImageFilter::updatedResolution(const QSize& srcSize)
{
    return m_size;
}

#endif // ENABLE_DATA_PROVIDERS
