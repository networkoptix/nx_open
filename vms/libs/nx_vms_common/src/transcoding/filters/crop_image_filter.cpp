// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crop_image_filter.h"

#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/log/log.h>
#include <transcoding/transcoding_utils.h>

extern "C" {
#ifdef WIN32
    #define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#ifdef WIN32
#undef AVPixFmtDescriptor
#endif
};

static QRectF cwiseMul(const QRectF &l, const QSizeF &r)
{
    return QRectF(
        l.left()   * r.width(),
        l.top()    * r.height(),
        l.width()  * r.width(),
        l.height() * r.height()
        );
}

QnCropImageFilter::QnCropImageFilter(const QRectF& rect, bool alignSize, bool deepCopy):
    m_deepCopy(deepCopy),
    m_alignSize(alignSize),
    m_rectF(rect)
{
}

QnCropImageFilter::QnCropImageFilter(const QRect& rect, bool alignSize, bool deepCopy):
    m_deepCopy(deepCopy),
    m_alignSize(alignSize),
    m_rect(rect)
{
}

CLVideoDecoderOutputPtr QnCropImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    if (m_rect.isEmpty() && m_rectF.isEmpty())
        return frame;

    if (!m_rectF.isNull() && (m_rect.isNull() || frame->size() != m_size))
    {
        m_size = frame->size();
        QRect rect(cwiseMul(m_rectF, frame->size()).toRect());
        if (m_alignSize)
            rect = nx::transcoding::roundRect(rect);
        const auto frameRect = QRect(0, 0, frame->width, frame->height);
        m_rect = rect.intersected(frameRect);
        if (m_rect.isEmpty())
            return frame;
    }

    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    if (m_deepCopy)
    {
        result->copyFrom(frame.get());
        result->crop_left = m_rect.left();
        result->crop_top = m_rect.top();
        result->crop_right = frame->width - m_rect.right();
        result->crop_bottom = frame->height - m_rect.bottom();
        if (const auto status = av_frame_apply_cropping(result.get(), 0); status < 0)
        {
            NX_WARNING(this, "Failed to crop frame, Ffmpeg error %1",
                nx::media::ffmpeg::avErrorToString(status));
        }
    }
    else
    {
        result.reset(new CLVideoDecoderOutput());
        result->setUseExternalData(true);
        m_tmpRef = frame; // keep object undeleted
        const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) frame->format);
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
    }
    return result;
}

QSize QnCropImageFilter::updatedResolution(const QSize& srcSize)
{
    QRect rect(cwiseMul(m_rectF, srcSize).toRect());
    if (rect.isEmpty())
        return srcSize;

    if (m_alignSize)
        rect = nx::transcoding::roundRect(rect);
    return rect.size();
}
