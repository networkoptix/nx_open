// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_helper.h"

#include <QtCore/QBuffer>
#include <QtCore/QDebug>

#include "nalUnits.h"
#include "bitStream.h"
#include "core/resource/storage_resource.h"
#include <utils/media/av_codec_helper.h>
#include <utils/media/utils.h>

#include <nx/audio/format.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/log/log.h>

extern "C" {

#include <libavutil/error.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

} // extern "C"

QnFfmpegHelper::StaticHolder QnFfmpegHelper::StaticHolder::instance;

namespace ffmpeg {

void logCallback(void* ptr, int level, const char* fmt, va_list vl)
{
    if (!fmt)
        return;

    char message[1024] = {0};
    switch (level)
    {
        case AV_LOG_WARNING:
        case AV_LOG_ERROR:
        case AV_LOG_FATAL:
        case AV_LOG_PANIC:
            if (vsnprintf(message, sizeof(message), fmt, vl) < 0)
                return;

            // Trim excess line ending.
            int size = strlen(message);
            if (size > 0 && message[size - 1] == '\n')
                message[size - 1] = 0;

            NX_DEBUG(NX_SCOPE_TAG, "%1", message);
            break;
    }
}

}

void QnFfmpegHelper::registerLogCallback()
{
    av_log_set_callback(&ffmpeg::logCallback);
}

void QnFfmpegHelper::copyAvCodecContextField(void** fieldPtr, const void* data, size_t size)
{
    NX_ASSERT(fieldPtr);

    av_freep(fieldPtr);

    if (size > 0)
    {
        NX_ASSERT(data);
        *fieldPtr = av_malloc(size);
        NX_ASSERT(*fieldPtr);
        memcpy(*fieldPtr, data, size);
    }
}

bool QnFfmpegHelper::isChromaPlane(int plane, const AVPixFmtDescriptor* avPixFmtDescriptor)
{
    // See the doc for AVComponentDescriptor (AVPixFmtDescriptor::comp).

    if (avPixFmtDescriptor->nb_components >= 3
        && !(avPixFmtDescriptor->flags & AV_PIX_FMT_FLAG_RGB))
    {
        // Plane 0 is luma, planes 1 and 2 are chroma.
        if (plane == 1 || plane == 2) //< A chroma plane, can have a reduced vertical resolution.
            return true;
    }

    // The plane is non-chroma (presumably, Y, RGB or A) - the plane's height equals the frame's.
    return false;
}

AVCodec* QnFfmpegHelper::findAvCodec(AVCodecID codecId)
{
    AVCodec* codec = avcodec_find_decoder(codecId);

    if (!codec || codec->id != codecId)
        return &StaticHolder::instance.avCodec;

    return codec;
}

//-------------------------------------------------------------------------------------------------

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    auto bytesRead = reader->read((char*) buf, size);
    return bytesRead == 0 ? AVERROR_EOF : bytesRead;
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    if (reader)
        return reader->write((char*) buf, size);
    else
        return 0;
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);

    qint64 absolutePos = pos;
    switch (whence)
    {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        absolutePos = reader->pos() + pos;
        break;
    case SEEK_END:
        absolutePos = reader->size() + pos;
        break;
    case 65536:
        return reader->size();
    default:
        return AVERROR(ENOENT);
    }

    return reader->seek(absolutePos);
}

AVIOContext* QnFfmpegHelper::createFfmpegIOContext(
    QnStorageResourcePtr resource,
    const QString& url,
    QIODevice::OpenMode openMode,
    int ioBlockSize)
{
    QString path = url;

    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    QIODevice* ioDevice = resource->open(path, openMode);
    if (ioDevice == 0)
        return 0;

    ioBuffer = (quint8*) av_malloc(ioBlockSize);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        ioBlockSize,
        (openMode & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

AVIOContext* QnFfmpegHelper::createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize)
{
    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    ioBuffer = (quint8*) av_malloc(ioBlockSize);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        ioBlockSize,
        (ioDevice->openMode() & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

qint64 QnFfmpegHelper::getFileSizeByIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        return ioDevice->size();
    }
    return 0;
}

void QnFfmpegHelper::closeFfmpegIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        avio_flush(ioContext);

        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        delete ioDevice;
        ioContext->opaque = 0;
        //avio_close2(ioContext);

        av_freep(&ioContext->buffer);
        av_opt_free(ioContext);
        av_free(ioContext);
    }
}

CodecParametersPtr QnFfmpegHelper::createVideoCodecParameters(
    const QnCompressedVideoData* data, const std::vector<uint8_t>* externalExtradata)
{
    auto codecParams = std::make_shared<CodecParameters>();
    auto avCodecParams = codecParams->getAvCodecParameters();
    avCodecParams->codec_type = AVMEDIA_TYPE_VIDEO;
    avCodecParams->codec_id = data->compressionType;
    if (!nx::media::fillExtraData(data, &avCodecParams->extradata, &avCodecParams->extradata_size))
    {
        if (!externalExtradata || externalExtradata->empty())
            return nullptr;

        codecParams->setExtradata(externalExtradata->data(), externalExtradata->size());
    }

    QSize streamResolution = nx::media::getFrameSize(data);
    if (streamResolution.isEmpty() && data->width > 8 && data->height > 8)
        streamResolution = QSize(data->width, data->height);

    if (!streamResolution.isEmpty())
    {
        avCodecParams->width = streamResolution.width();
        avCodecParams->height = streamResolution.height();
    }
    return codecParams;
}

std::string QnFfmpegHelper::avErrorToString(int errorCode)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errorCode, errorBuffer, AV_ERROR_MAX_STRING_SIZE);
    return errorBuffer;
}

int QnFfmpegHelper::audioSampleSize(AVCodecContext* ctx)
{
    return ctx->channels * av_get_bytes_per_sample(ctx->sample_fmt);
}

AVSampleFormat QnFfmpegHelper::fromAudioFormatToFfmpegSampleType(const nx::audio::Format& format)
{
    if (format.sampleType == nx::audio::Format::SampleType::signedInt)
    {
        if (format.sampleSize == 16)
            return AV_SAMPLE_FMT_S16;

        if (format.sampleSize == 32)
            return AV_SAMPLE_FMT_S32;
    }
    else if (format.sampleType == nx::audio::Format::SampleType::unsignedInt)
    {
        if (format.sampleSize == 8)
            return AV_SAMPLE_FMT_U8;
    }
    else if (format.sampleType == nx::audio::Format::SampleType::floatingPoint)
    {
        if (format.sampleSize == 32)
            return AV_SAMPLE_FMT_FLT;

        if (format.sampleSize == 64)
            return AV_SAMPLE_FMT_DBL;
    }

    return AV_SAMPLE_FMT_NONE;
}

int QnFfmpegHelper::getDefaultFrameSize(AVCodecParameters* avCodecParams)
{
    AVCodec* avCodec = avcodec_find_encoder(avCodecParams->codec_id);
    if (!avCodec)
        return 0;

    auto encoderContext = avcodec_alloc_context3(avCodec);

    if (avCodec->sample_fmts)
        encoderContext->sample_fmt = avCodec->sample_fmts[0];
    encoderContext->channels = avCodecParams->channels;
    encoderContext->sample_rate = avCodecParams->sample_rate;
    encoderContext->channel_layout = av_get_default_channel_layout(encoderContext->channels);
    encoderContext->bit_rate = 64'000;
    auto status = avcodec_open2(encoderContext, avCodec, nullptr);
    auto res = status >= 0 ? encoderContext->frame_size : 0;
    avcodec_free_context(&encoderContext);
    return res;
}

int QnFfmpegHelper::planeCount(const AVPixFmtDescriptor* avPixFmtDescriptor)
{
    // As per the doc for AVComponentDescriptor (AVPixFmtDescriptor::comp), each pixel has 1 to 4
    // so-called components (e.g. R, G, B, Y, U, V or A), each component resides in a certain plane
    // (0..3), and a plane can host more than one component (e.g. for RGB24 the only plane #0 hosts
    // all 3 components).

    // Find the max plane index for all components.
    int maxPlane = -1;
    for (int component = 0; component < avPixFmtDescriptor->nb_components; ++component)
    {
        const int componentPlane = avPixFmtDescriptor->comp[component].plane;
        if (!NX_ASSERT(componentPlane >= 0 && componentPlane < AV_NUM_DATA_POINTERS,
            NX_FMT("AVPixFmtDescriptor reports plane %1 for component %2 of pixel format %3"),
                componentPlane, component, avPixFmtDescriptor->name))
        {
            return 0;
        }

        if (componentPlane > maxPlane)
            maxPlane = componentPlane;
    }

    return maxPlane + 1;
}

QnFfmpegAudioHelper::QnFfmpegAudioHelper(AVCodecContext* decoderContext):
    m_swr(swr_alloc())
{
    av_opt_set_int(m_swr, "in_channel_layout", decoderContext->channel_layout, 0);
    av_opt_set_int(m_swr, "out_channel_layout", decoderContext->channel_layout, 0);
    av_opt_set_int(m_swr, "in_channel_count", decoderContext->channels, 0);
    av_opt_set_int(m_swr, "out_channel_count", decoderContext->channels, 0);
    av_opt_set_int(m_swr, "in_sample_rate", decoderContext->sample_rate, 0);
    av_opt_set_int(m_swr, "out_sample_rate", decoderContext->sample_rate, 0);
    av_opt_set_sample_fmt(m_swr, "in_sample_fmt", decoderContext->sample_fmt, 0);
    av_opt_set_sample_fmt(m_swr, "out_sample_fmt",
        av_get_packed_sample_fmt(decoderContext->sample_fmt), 0);
    swr_init(m_swr);

    m_channel_layout = decoderContext->channel_layout;
    m_channels = decoderContext->channels;
    m_sample_rate = decoderContext->sample_rate;
    m_sample_fmt = decoderContext->sample_fmt;
}

bool QnFfmpegAudioHelper::isCompatible(AVCodecContext* decoderContext) const
{
    return m_channel_layout == decoderContext->channel_layout
        && m_channels == decoderContext->channels
        && m_sample_rate == decoderContext->sample_rate
        && m_sample_fmt == decoderContext->sample_fmt;
}

QnFfmpegAudioHelper::~QnFfmpegAudioHelper()
{
    swr_free(&m_swr);
}

void QnFfmpegAudioHelper::copyAudioSamples(quint8* dst, const AVFrame* src) const
{
    quint8* tmpData[4] = {dst};
    swr_convert(m_swr,tmpData, src->nb_samples, (const quint8**) src->data, src->nb_samples);
}

QnFfmpegAvPacket::QnFfmpegAvPacket(uint8_t* data, int size)
{
    av_init_packet(this);
    this->data = data;
    this->size = size;
}

QnFfmpegAvPacket::~QnFfmpegAvPacket()
{
    av_packet_unref(this);
}

QString toString(AVPixelFormat pixelFormat)
{
    const AVPixFmtDescriptor* const descriptor = av_pix_fmt_desc_get(pixelFormat);
    if (!descriptor || !descriptor->name)
        return "AVPixelFormat(" + QString::number((int) pixelFormat) + ")";
    return descriptor->name;
}
