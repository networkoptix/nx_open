// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
} // extern "C"

#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/media_data_packet.h>

namespace nx::media::audio { struct Format; }

class QnCompressedVideoData;
/** Static.
 * Contains utilities which rely on ffmpeg implementation.
 */
class NX_MEDIA_CORE_API QnFfmpegHelper
{
public:
    /**
     * Copy new value (array of bytes) to a field of an ffmpeg structure (e.g. AVCodecContext),
     * deallocating if not null, then allocating via av_malloc().
     * If size is 0, data can be null.
     */
    static void copyAvCodecContextField(void **fieldPtr, const void* data, size_t size);

    static CodecParametersPtr createVideoCodecParametersAnnexB(
        const QnCompressedVideoData* data, const std::vector<uint8_t>* externalExtradata = nullptr);

    static CodecParametersPtr createVideoCodecParametersMp4(
        const QnCompressedVideoData* video, int width, int height);

    static qint64 getFileSizeByIOContext(AVIOContext* ioContext);

    static void registerLogCallback();

    static int audioSampleSize(AVCodecContext* ctx);

    static AVSampleFormat fromAudioFormatToFfmpegSampleType(const nx::media::audio::Format& format);

    static int getDefaultFrameSize(AVCodecParameters* codecParams);

    /**
     * @return Whether the plane with the specified index is a chroma plane.
     */
    static bool isChromaPlane(int plane, const AVPixFmtDescriptor* avPixFmtDescriptor);

    /**
     * Ffmpeg does not store the number of planes in a frame explicitly, thus, deducing it. On error,
     * fails an assertion and returns 0.
     */
    static int planeCount(const AVPixFmtDescriptor* avPixFmtDescriptor);
};

struct SwrContext;

class NX_MEDIA_CORE_API QnFfmpegAudioHelper
{
public:
    QnFfmpegAudioHelper(AVCodecContext* decoderContext);
    ~QnFfmpegAudioHelper();

    void copyAudioSamples(quint8* dst, const AVFrame* src) const;
    bool isCompatible(AVCodecContext* decoderContext) const;

private:
    SwrContext* m_swr = nullptr;
    int m_channel_layout = 0;
    int m_channels = 0;
    int m_sample_rate = 0;
    int m_sample_fmt = 0;
};

NX_MEDIA_CORE_API QByteArray toString(AVPixelFormat pixelFormat);
