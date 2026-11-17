// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QSize>

#include <nx/media/ffmpeg/frame_info.h>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx::media::ffmpeg { class AvOptions; }

namespace nx::transcoding {

/**
 * Encodes on the hardware device when given one that has an encoder for the codec, in software
 * otherwise. The choice is final once open: the muxer takes the codec parameters from there.
 * Takes frames in system or video memory, and uploads or downloads them as the encoder needs.
 */
class NX_VMS_COMMON_API VideoEncoder
{
public:
    // Sets the codec-independent parameters; context->codec tells which encoder it is.
    using Configure =
        std::function<void(AVCodecContext* context, nx::media::ffmpeg::AvOptions& options)>;

    VideoEncoder() = default;
    ~VideoEncoder();

    VideoEncoder(const VideoEncoder&) = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;

    bool open(AVCodecID codec, const QSize& size, AVBufferRef* device, const Configure& configure);
    void close();

    AVCodecContext* context() const { return m_context; }
    bool isHardware() const { return m_context && m_context->hw_frames_ctx; }

    // Null frame drains the encoder.
    int encode(const CLVideoDecoderOutputPtr& frame, AVPacket* packet, int* gotPacket);

private:
    bool openCodec(
        const AVCodec* codec, const QSize& size, AVBufferRef* device, const Configure& configure);
    CLVideoDecoderOutputPtr toEncoderMemory(const CLVideoDecoderOutputPtr& frame) const;

private:
    AVCodecContext* m_context = nullptr;
};

} // namespace nx::transcoding
