// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_encoder.h"

extern "C" {
#include <libavutil/hwcontext.h>
} // extern "C"

#include <nx/media/ffmpeg/av_options.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/media/ffmpeg/old_api.h>
#include <nx/utils/log/log.h>

namespace nx::transcoding {

VideoEncoder::~VideoEncoder()
{
    close();
}

void VideoEncoder::close()
{
    avcodec_free_context(&m_context);
}

bool VideoEncoder::open(
    AVCodecID codecId, const QSize& size, AVBufferRef* device, const Configure& configure)
{
    close();
    const char* const hardwareName = !device ? nullptr
        : codecId == AV_CODEC_ID_H264        ? "h264_vaapi"
        : codecId == AV_CODEC_ID_HEVC        ? "hevc_vaapi"
        : codecId == AV_CODEC_ID_AV1         ? "av1_vaapi"
                                             : nullptr;
    if (hardwareName)
    {
        if (const AVCodec* codec = avcodec_find_encoder_by_name(hardwareName);
            codec && openCodec(codec, size, device, configure))
        {
            return true;
        }
        NX_WARNING(this, "Unable to open %1, encoding in software", hardwareName);
    }

    // Hardware encoders are listed after the software ones, so a hardware one here means there
    // is no software encoder, and it can't be opened without a device.
    const AVCodec* codec = avcodec_find_encoder(codecId);
    if (!codec || (codec->capabilities & AV_CODEC_CAP_HARDWARE))
    {
        NX_WARNING(this, "No software encoder for codec %1", codecId);
        return false;
    }
    return openCodec(codec, size, /*device*/ nullptr, configure);
}

bool VideoEncoder::openCodec(
    const AVCodec* codec, const QSize& size, AVBufferRef* device, const Configure& configure)
{
    close();
    m_context = avcodec_alloc_context3(codec);
    if (!m_context)
        return false;

    m_context->width = size.width();
    m_context->height = size.height();
    m_context->pix_fmt = device          ? AV_PIX_FMT_VAAPI
        : codec->id == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P
                                         : AV_PIX_FMT_YUV420P;
    if (device)
    {
        m_context->max_b_frames = 0;
        m_context->hw_frames_ctx = av_hwframe_ctx_alloc(device);
        if (!m_context->hw_frames_ctx)
        {
            close();
            return false;
        }
        auto frames = (AVHWFramesContext*) m_context->hw_frames_ctx->data;
        frames->format = AV_PIX_FMT_VAAPI;
        frames->sw_format = AV_PIX_FMT_NV12;
        frames->width = size.width();
        frames->height = size.height();
        if (const int status = av_hwframe_ctx_init(m_context->hw_frames_ctx); status < 0)
        {
            NX_WARNING(this,
                "Unable to init encoder frames: %1",
                nx::media::ffmpeg::avErrorToString(status));
            close();
            return false;
        }
    }

    nx::media::ffmpeg::AvOptions options;
    configure(m_context, options);
    if (const int status = avcodec_open2(m_context, codec, options); status < 0)
    {
        NX_WARNING(this,
            "Unable to open encoder %1: %2",
            codec->name,
            nx::media::ffmpeg::avErrorToString(status));
        close();
        return false;
    }
    NX_DEBUG(this, "Encoding with %1", codec->name);
    return true;
}

CLVideoDecoderOutputPtr VideoEncoder::toEncoderMemory(const CLVideoDecoderOutputPtr& frame) const
{
    if (isHardware() == (bool) frame->hw_frames_ctx)
        return frame;

    if (!isHardware())
    {
        auto result = frame->toSystemMemory();
        if (result && result->format != m_context->pix_fmt)
            result = result->scaled(result->size(), m_context->pix_fmt);
        return result;
    }

    auto source = frame;
    if (frame->format != AV_PIX_FMT_NV12
        || frame->size() != QSize(m_context->width, m_context->height))
        source = frame->scaled(QSize(m_context->width, m_context->height), AV_PIX_FMT_NV12);
    if (!source)
        return {};

    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    if (const int status = av_hwframe_get_buffer(m_context->hw_frames_ctx, result.data(), 0);
        status < 0)
    {
        NX_WARNING(this,
            "Unable to allocate a frame on the device: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return {};
    }
    if (const int status = av_hwframe_transfer_data(result.data(), source.data(), 0); status < 0)
    {
        NX_WARNING(this,
            "Unable to upload a frame to the device: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return {};
    }
    result->assignMiscData(frame.data());
    return result;
}

int VideoEncoder::encode(const CLVideoDecoderOutputPtr& frame, AVPacket* packet, int* gotPacket)
{
    *gotPacket = 0;
    CLVideoDecoderOutputPtr input;
    if (frame)
    {
        input = toEncoderMemory(frame);
        if (!input)
            return 0; //< Drop the frame, as the filters do when they fail.
    }
    return nx::media::ffmpeg::old_api::encode(m_context, packet, input.data(), gotPacket);
}

} // namespace nx::transcoding
