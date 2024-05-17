// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rotate_image_filter.h"

#include <utils/media/frame_info.h>

CLVideoDecoderOutputPtr rotateImage(const CLConstVideoDecoderOutputPtr& frame, int angle)
{
    if (angle > 180)
        angle = 270;
    else if (angle > 90)
        angle = 180;
    else
        angle = 90;

    int dstWidth = frame->width;
    int dstHeight = frame->height;
    if (angle != 180)
        qSwap(dstWidth, dstHeight);

    bool transposeChroma = false;
    if (angle == 90 || angle == 270) {
        if (frame->format == AV_PIX_FMT_YUV422P || frame->format == AV_PIX_FMT_YUVJ422P)
            transposeChroma = true;
    }

    CLVideoDecoderOutputPtr dstPict(new CLVideoDecoderOutput());
    dstPict->reallocate(dstWidth, dstHeight, frame->format);
    dstPict->assignMiscData(frame.get());

    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get((AVPixelFormat) frame->format);
    for (int i = 0; i < QnFfmpegHelper::planeCount(descriptor) && frame->data[i]; ++i)
    {
        int filler = (i == 0 ? 0x0 : 0x80);
        int numBytes = dstPict->linesize[i] * dstHeight;
        const bool isChromaPlane = QnFfmpegHelper::isChromaPlane(i, descriptor);
        if (isChromaPlane)
            numBytes >>= descriptor->log2_chroma_h;
        memset(dstPict->data[i], filler, numBytes);

        int w = frame->width;
        int h = frame->height;

        if (isChromaPlane && !transposeChroma)
        {
            w >>= descriptor->log2_chroma_w;
            h >>= descriptor->log2_chroma_h;
        }

        if (angle == 90)
        {
            int dstLineStep = dstPict->linesize[i];

            if (transposeChroma && isChromaPlane)
            {
                for (int y = 0; y < h; y += 2)
                {
                    quint8* src = frame->data[i] + frame->linesize[i] * y;
                    quint8* dst = dstPict->data[i] + (h - y)/2 - 1;
                    for (int x = 0; x < w/2; ++x) {
                        quint8 pixel = ((quint16) src[0] + (quint16) src[frame->linesize[i]]) >> 1;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        src++;
                    }
                }
            }
            else {
                for (int y = 0; y < h; ++y)
                {
                    quint8* src = frame->data[i] + frame->linesize[i] * y;
                    quint8* dst = dstPict->data[i] + h - y -1;
                    if (angle == 270)
                        dst += (w-1) * dstPict->linesize[i];
                    for (int x = 0; x < w; ++x) {
                        *dst = *src++;
                        dst += dstLineStep;
                    }
                }
            }
        }
        else if (angle == 270)
        {
            int dstLineStep = -dstPict->linesize[i];

            if (transposeChroma && isChromaPlane)
            {
                for (int y = 0; y < h; y += 2)
                {
                    quint8* src = frame->data[i] + frame->linesize[i] * y;
                    quint8* dst = dstPict->data[i] + (w-1) * dstPict->linesize[i] + y/2;
                    for (int x = 0; x < w/2; ++x) {
                        quint8 pixel = ((quint16) src[0] + (quint16) src[frame->linesize[i]]) >> 1;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        src++;
                    }
                }
            }
            else {
                for (int y = 0; y < h; ++y)
                {
                    quint8* src = frame->data[i] + frame->linesize[i] * y;
                    quint8* dst = dstPict->data[i] + (w-1) * dstPict->linesize[i] + y;
                    for (int x = 0; x < w; ++x) {
                        *dst = *src++;
                        dst += dstLineStep;
                    }
                }
            }
        }
        else
        {  // 180
            for (int y = 0; y < h; ++y) {
                quint8* src = frame->data[i] + frame->linesize[i] * y;
                quint8* dst = dstPict->data[i] + dstPict->linesize[i] * (h-1 - y) + w-1;
                for (int x = 0; x < w; ++x) {
                    *dst-- = *src++;
                }
            }
        }
    }

    return dstPict;
}

QnRotateImageFilter::QnRotateImageFilter(int angle): m_angle(angle)
{
    // normalize value
    m_angle = m_angle % 360;
    if (m_angle < 0)
        m_angle += 360;
    m_angle = int(m_angle / 90.0 + 0.5) * 90;
    m_angle = m_angle % 360;
}

CLVideoDecoderOutputPtr QnRotateImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame,
    const QnAbstractCompressedMetadataPtr&)
{
    if (m_angle == 0)
        return frame;
    else
        return rotateImage(frame, m_angle);
}

QSize QnRotateImageFilter::updatedResolution(const QSize& srcSize)
{
    QSize dstSize(srcSize);
    if (m_angle == 90 || m_angle == 270)
        dstSize.transpose();
    return dstSize;
}
