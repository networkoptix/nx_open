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

CLVideoDecoderOutputPtr QnCropImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    if (m_rect.isEmpty())
        return frame;
    m_tmpRef = frame; // keep object undeleted
    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    result->setUseExternalData(true);

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[frame->format];
    for (int i = 0; i < descr->nb_components && frame->data[i]; ++i)
    {
        int w = m_rect.left();
        int h = m_rect.top();
        if (i > 0) {
            w >>= descr->log2_chroma_w;
            h >>= descr->log2_chroma_h;
        }
        result->data[i] = frame->data[i] + w + h * frame->linesize[i];
        result->linesize[i] = frame->linesize[i];
    }
    result->format = frame->format;
    result->width = m_rect.width();
    result->height = m_rect.height();

    result->assignMiscData(frame.data());
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
