// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixel_format_converter.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <string>

#include <nx/utils/log/log.h>

namespace nx::media {

namespace {

static const nx::log::Tag kLogTag(QString("PixelFormatConverter"));

void logConversionFailure(const AVFrame* srcFrame, const std::string& reason)
{
    const auto srcFormatName = av_get_pix_fmt_name((AVPixelFormat) srcFrame->format);
    NX_WARNING(kLogTag,
        "Failed to convert pixel format %1 to YUV420P: %2",
        srcFormatName ? srcFormatName : "unknown",
        reason);
}

} // namespace

PixelFormatConverter::~PixelFormatConverter()
{
    if (m_scaleContext)
        sws_freeContext(m_scaleContext);
}

AVFrame* PixelFormatConverter::toYuv420p(const AVFrame* srcFrame)
{
    static const AVPixelFormat dstAvFormat = AV_PIX_FMT_YUV420P;

    m_scaleContext = sws_getCachedContext(m_scaleContext,
        srcFrame->width,
        srcFrame->height,
        (AVPixelFormat) srcFrame->format,
        srcFrame->width,
        srcFrame->height,
        dstAvFormat,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr);

    if (!m_scaleContext) //< ffmpeg can return null context.
    {
        logConversionFailure(srcFrame, "swscale context unavailable");
        return nullptr;
    }

    AVFrame* dstFrame = av_frame_alloc();
    if (!dstFrame)
    {
        logConversionFailure(srcFrame, "frame allocation failed");
        return nullptr;
    }

    int numBytes = av_image_get_buffer_size(
        dstAvFormat, srcFrame->linesize[0], srcFrame->height, /*align*/ 1);
    if (numBytes <= 0)
    {
        av_frame_free(&dstFrame);
        logConversionFailure(srcFrame, "invalid destination buffer size");
        return nullptr;
    }
    numBytes += AV_INPUT_BUFFER_PADDING_SIZE; //< Extra alloc space due to ffmpeg doc.

    dstFrame->buf[0] = av_buffer_alloc(numBytes);
    if (!dstFrame->buf[0])
    {
        av_frame_free(&dstFrame);
        logConversionFailure(srcFrame, "destination buffer allocation failed");
        return nullptr;
    }

    dstFrame->width = srcFrame->width;
    dstFrame->height = srcFrame->height;
    dstFrame->format = dstAvFormat;

    if (av_image_fill_arrays(dstFrame->data,
            dstFrame->linesize,
            dstFrame->buf[0]->data,
            dstAvFormat,
            srcFrame->linesize[0],
            srcFrame->height,
            /*align*/ 1)
        < 0)
    {
        av_frame_free(&dstFrame);
        logConversionFailure(srcFrame, "image array fill failed");
        return nullptr;
    }

    sws_scale(m_scaleContext,
        srcFrame->data,
        srcFrame->linesize,
        0,
        srcFrame->height,
        dstFrame->data,
        dstFrame->linesize);

    return dstFrame;
}

} // namespace nx::media
