// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "core/resource/resource_fwd.h"

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavutil/mem.h>
#include <libavutil/imgutils.h>

} // extern "C"

#include <QtCore/QIODevice>

#include <nx/streaming/config.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/media_data_packet.h>

namespace nx::audio { struct Format; }

class QnCompressedVideoData;
/** Static.
 * Contains utilities which rely on ffmpeg implementation.
 */
class NX_VMS_COMMON_API QnFfmpegHelper
{
public:
    /**
     * Copy new value (array of bytes) to a field of an ffmpeg structure (e.g. AVCodecContext),
     * deallocating if not null, then allocating via av_malloc().
     * If size is 0, data can be null.
     */
    static void copyAvCodecContextField(void **fieldPtr, const void* data, size_t size);

    static CodecParametersPtr createVideoCodecParameters(
        const QnCompressedVideoData* data, const std::vector<uint8_t>* externalExtradata = nullptr);

    /**
     * @return Either a codec found in ffmpeg registry, or a static instance of a stub AVCodec in case
     * the proper codec is not available in ffmpeg; never null.
     */
    static AVCodec* findAvCodec(AVCodecID codecId);

    static AVIOContext* createFfmpegIOContext(QnStorageResourcePtr resource, const QString& url, QIODevice::OpenMode openMode, int ioBlockSize = 32768);
    static AVIOContext* createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize = 32768);
    static void closeFfmpegIOContext(AVIOContext* ioContext);
    static qint64 getFileSizeByIOContext(AVIOContext* ioContext);

    static std::string avErrorToString(int errorCode);
    static void registerLogCallback();

    static int audioSampleSize(AVCodecContext* ctx);

    static AVSampleFormat fromAudioFormatToFfmpegSampleType(const nx::audio::Format& format);

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

private:
    /**
     * Holds a globally shared static instance of a stub AVCodec used for
     * AVCodecContext when the proper codec is not available in ffmpeg.
     */
    class StaticHolder
    {
    public:
        // Instance is not const because ffmpeg requires non-const AVCodec.
        static StaticHolder instance;

        AVCodec avCodec;

    private:
        StaticHolder()
        {
            memset(&avCodec, 0, sizeof(avCodec));
            avCodec.id = AV_CODEC_ID_NONE;
            avCodec.type = AVMEDIA_TYPE_VIDEO;
        }

        StaticHolder(const StaticHolder&);
        void operator=(const StaticHolder&);
    };
};

struct SwrContext;

class NX_VMS_COMMON_API QnFfmpegAudioHelper
{
public:
    QnFfmpegAudioHelper(AVCodecContext* decoderContext);
    ~QnFfmpegAudioHelper();

    void copyAudioSamples(quint8* dst, const AVFrame* src) const;

private:
    SwrContext* m_swr;
};

struct NX_VMS_COMMON_API QnFfmpegAvPacket: AVPacket //< TODO: #lbusygin Remove deprecated.
{
    QnFfmpegAvPacket(uint8_t* data = nullptr, int size = 0);
    ~QnFfmpegAvPacket();

    QnFfmpegAvPacket(const QnFfmpegAvPacket&) = delete;
    QnFfmpegAvPacket& operator=(const QnFfmpegAvPacket&) = delete;
};

NX_VMS_COMMON_API QString toString(AVPixelFormat pixelFormat);
