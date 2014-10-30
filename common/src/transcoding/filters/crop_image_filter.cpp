#include "crop_image_filter.h"
#include "utils/media/frame_info.h"

#ifdef ENABLE_DATA_PROVIDERS

extern "C" {
#ifdef WIN32
#define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#ifdef WIN32
#undef AVPixFmtDescriptor
#endif
};


QnCropImageFilter::QnCropImageFilter(const QRect& rect): m_rect(rect)
{
    
}

CLVideoDecoderOutputPtr QnCropImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar)
{
    if (m_rect.isEmpty())
        return frame;
    QRect frameRect;
    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    result->setUseExternalData(true);

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[frame->format];
    for (int i = 0; i < descr->nb_components && frame->data[i]; ++i)
    {
        int w = frameRect.left();
        int h = frameRect.top();
        if (i > 0) {
            w >>= descr->log2_chroma_w;
            h >>= descr->log2_chroma_h;
        }
        result->data[i] = frame->data[i] + w + h * frame->linesize[i];
        result->linesize[i] = frame->linesize[i];
    }
    result->format = frame->format;
    result->width = frameRect.width();
    result->height = frameRect.height();

    return result;
}

QSize QnCropImageFilter::updatedResolution(const QSize& srcSize)
{
    if (m_rect.isEmpty())
        return srcSize;
    else
        return m_rect.size();
}

#endif // ENABLE_DATA_PROVIDERS
