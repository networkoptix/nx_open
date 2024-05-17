// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crop_image_filter.h"

#include <utils/media/frame_info.h>

#include "../transcoder.h"

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

QnCropImageFilter::QnCropImageFilter(const QRectF& rect, bool alignSize):
    m_alignSize(alignSize),
    m_rectF(rect)
{
}

QnCropImageFilter::QnCropImageFilter(const QRect& rect, bool alignSize):
    m_alignSize(alignSize),
    m_rect(rect)
{
}

CLVideoDecoderOutputPtr QnCropImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame,
    const QnAbstractCompressedMetadataPtr&)
{
    if (m_rect.isEmpty() && m_rectF.isEmpty())
        return frame;
    m_tmpRef = frame; // keep object undeleted
    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    result->setUseExternalData(true);

    if (!m_rectF.isNull() && (m_rect.isNull() || frame->size() != m_size))
    {
        m_size = frame->size();
        QRect rect(cwiseMul(m_rectF, frame->size()).toRect());
        if (m_alignSize)
            rect = QnCodecTranscoder::roundRect(rect);
        const auto frameRect = QRect(0, 0, frame->width, frame->height);
        m_rect = rect.intersected(frameRect);
        if (m_rect.isEmpty())
            return frame;
    }

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
    return result;
}

QSize QnCropImageFilter::updatedResolution(const QSize& srcSize)
{
    QRect rect(cwiseMul(m_rectF, srcSize).toRect());
    if (rect.isEmpty())
        return srcSize;

    if (m_alignSize)
        rect = QnCodecTranscoder::roundRect(rect);
    return rect.size();
}
