// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_muxer.h"

extern "C" {
#include <libavutil/opt.h>
}

#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/media/utils.h>
#include <nx/utils/log/log.h>
#include <utils/common/util.h>
#include <utils/media/ffmpeg_io_context.h>

namespace {

static AVPixelFormat jpegPixelFormatToRtp(AVPixelFormat value)
{
    switch (value)
    {
        case AV_PIX_FMT_YUV420P: return AV_PIX_FMT_YUVJ420P;
        case AV_PIX_FMT_YUV422P: return AV_PIX_FMT_YUVJ422P;
        case AV_PIX_FMT_YUV444P: return AV_PIX_FMT_YUVJ444P;
        default: return value;
    }
}

static AVPixelFormat getPixelFormatJpeg(const std::string_view container, AVPixelFormat format)
{
    if (container == "rtp")
        return jpegPixelFormatToRtp(format);
    else
        return format;
}

}

FfmpegMuxer::FfmpegMuxer(const Config& config):
    m_config(config),
    m_internalBuffer(CL_MEDIA_ALIGNMENT, 1024*1024, AV_INPUT_BUFFER_PADDING_SIZE),
    m_timestampCorrector(std::chrono::milliseconds(MAX_FRAME_DURATION_MS), std::chrono::milliseconds(1))
{
}

FfmpegMuxer::~FfmpegMuxer()
{
    closeFfmpegContext();
}

bool FfmpegMuxer::isCodecSupported(AVCodecID id) const
{
    if (!m_formatCtx || !m_formatCtx->oformat)
        return false;

    return avformat_query_codec(m_formatCtx->oformat, id, FF_COMPLIANCE_NORMAL) == 1;
}

bool FfmpegMuxer::process(const QnConstAbstractMediaDataPtr& media)
{
    m_outputPacketSize.clear();
    if (media->dataType != QnAbstractMediaData::VIDEO && media->dataType != QnAbstractMediaData::AUDIO)
        return true; // mux only audio and video, skip packet

    if ((media->dataType == QnAbstractMediaData::AUDIO && !m_initializedAudio) ||
        (media->dataType == QnAbstractMediaData::VIDEO && !m_initializedVideo))
    {
        return true;
    }

    return muxPacket(media);
}

void FfmpegMuxer::setPacketizedMode(bool value)
{
    m_packetizedMode = value;
}

const QVector<int>& FfmpegMuxer::getPacketsSize()
{
    return m_outputPacketSize;
}

void FfmpegMuxer::closeFfmpegContext()
{
    if (m_formatCtx)
    {
        if (m_initialized)
            av_write_trailer(m_formatCtx);
        m_initialized = false;
        m_ioContext.reset();
        m_formatCtx->pb = nullptr;
        avformat_close_input(&m_formatCtx);
    }
    m_timestampCorrector.clear();
}

bool FfmpegMuxer::setContainer(const QString& container)
{
    m_container = container;
    const AVOutputFormat* outputCtx = av_guess_format(m_container.toLatin1().data(), NULL, NULL);
    if (outputCtx == 0)
    {
        NX_WARNING(this, "Container %1 was not found in FFMPEG library.", container);
        return false;
    }

    int err = avformat_alloc_output_context2(&m_formatCtx, outputCtx, 0, "");
    if (err != 0)
    {
        NX_WARNING(this, "Could not create output context for format %1.", container);
        return false;
    }
    if (container == QLatin1String("rtp"))
        m_formatCtx->packet_size = m_rtpMtu;

    return true;
}

void FfmpegMuxer::setFormatOption(const QString& option, const QString& value)
{
    av_opt_set(m_formatCtx->priv_data, option.toLatin1().data(), value.toLatin1().data(), 0);
}

bool FfmpegMuxer::addVideo(const AVCodecParameters* codecParameters)
{
    if (!m_formatCtx || !codecParameters)
        return false;

    NX_DEBUG(this, "Add video %1.", codecParameters->codec_id);
    AVStream* videoStream = avformat_new_stream(m_formatCtx, nullptr);
    if (videoStream == 0)
    {
        NX_ERROR(this, "Could not allocate output stream for recording.");
        return false;
    }

    m_videoCodecParameters = videoStream->codecpar;
    avcodec_parameters_copy(m_videoCodecParameters, codecParameters);
    m_videoCodecParameters->codec_tag = 0; //< Zero tag since mp4 does not support tags.

    if (m_videoCodecParameters->codec_id == AV_CODEC_ID_MJPEG)
    {
        m_videoCodecParameters->format = getPixelFormatJpeg(
            m_container.toStdString(), (AVPixelFormat)m_videoCodecParameters->format);
    }

    m_videoCodecParameters->sample_aspect_ratio.num = 1;
    m_videoCodecParameters->sample_aspect_ratio.den = 1;

    videoStream->id = 0;
    videoStream->time_base.num = 1;
    videoStream->time_base.den = 60;
    videoStream->sample_aspect_ratio = m_videoCodecParameters->sample_aspect_ratio;
    m_initializedVideo = true;
    return true;
}

bool FfmpegMuxer::addAudio(const AVCodecParameters* codecParameters)
{
    if (!m_formatCtx || !codecParameters)
        return false;

    NX_DEBUG(this, "Add audio %1.", codecParameters->codec_id);
    AVStream* audioStream = avformat_new_stream(m_formatCtx, nullptr);
    if (audioStream == 0)
    {
        NX_ERROR(this, "Could not allocate output stream for recording.");
        return false;
    }

    audioStream->id = 0;
    m_audioCodecParameters = audioStream->codecpar;
    avcodec_parameters_copy(m_audioCodecParameters, codecParameters);
    m_initializedAudio = true;
    return true;
}

bool FfmpegMuxer::open(std::optional<QnAviArchiveMetadata> metadata)
{
    if (m_formatCtx->nb_streams == 0)
        return false;

    m_ioContext = std::make_unique<nx::media::ffmpeg::IoContext>(
        /*bufferSize*/ 1024 * 16, /*writable*/ true, /*seekable*/ false);
    m_ioContext->writeHandler = [this](const uint8_t* data, int size)
        {
            m_internalBuffer.write((const char*)data, size);
            if (m_packetizedMode)
                m_outputPacketSize << size;
            return size;
        };

    m_formatCtx->pb = m_ioContext->getAvioContext();
    if (metadata)
        metadata->saveToFile(m_formatCtx, metadata->formatFromExtension(m_container));
    int rez = avformat_write_header(m_formatCtx, 0);
    if (rez < 0)
    {
        closeFfmpegContext();
        NX_ERROR(this, "Video or audio codec is incompatible with container %1.", m_container);
        return false;
    }
    m_initialized = true;
    return true;
}

bool FfmpegMuxer::addTag(const char* name, const char* value)
{
    return av_dict_set(&m_formatCtx->metadata, name, value, 0) >= 0;
}

void FfmpegMuxer::setStartTimeOffset(int64_t value)
{
    m_timestampCorrector.setOffset(std::chrono::microseconds(value));
}

bool FfmpegMuxer::muxPacket(const QnConstAbstractMediaDataPtr& media)
{
    int streamIndex = 0;
    if (m_initializedVideo && media->dataType == QnAbstractMediaData::AUDIO)
       streamIndex = 1;

    if (streamIndex >= (int)m_formatCtx->nb_streams)
    {
        NX_DEBUG(this, "Invalid packet media type: %1, skip it", media->dataType);
        return true;
    }

    AVStream* stream = m_formatCtx->streams[streamIndex];
    constexpr AVRational srcRate = {1, 1'000'000};
    nx::media::ffmpeg::AvPacket avPacket;
    auto packet = avPacket.get();

    bool allowEqualTimestamps = media->dataType == QnAbstractMediaData::GENERIC_METADATA;
    auto timestamp = m_timestampCorrector.process(
        std::chrono::microseconds(media->timestamp),
        streamIndex,
        allowEqualTimestamps,
        media->flags & QnAbstractMediaData::MediaFlags_Discontinuity).count();

    packet->pts = av_rescale_q(timestamp, srcRate, stream->time_base);
    packet->data = (uint8_t*)media->data();
    packet->size = static_cast<int>(media->dataSize());
    if (media->dataType == QnAbstractMediaData::AUDIO || media->flags & QnAbstractMediaData::MediaFlags_AVKey)
        packet->flags |= AV_PKT_FLAG_KEY;

    packet->stream_index = streamIndex;
    packet->dts = packet->pts;

    m_lastPacketTimestamp.ntpTimestamp = media->timestamp;
    m_lastPacketTimestamp.rtpTimestamp = packet->pts;

    int status = av_write_frame(m_formatCtx, packet);
    if (status < 0)
    {
        NX_WARNING(this, "Muxing packet error: can't write AV packet: %1, error: %2",
            media, nx::media::ffmpeg::avErrorToString(status));
        return false;
    }

    if (m_config.computeSignature)
    {
        auto context = media->dataType == QnAbstractMediaData::VIDEO ?
            m_videoCodecParameters : m_audioCodecParameters;
        m_mediaSigner.processMedia(context, packet->data, packet->size);
    }
    return true;
}

bool FfmpegMuxer::finalize()
{
    closeFfmpegContext();
    return true;
}

void FfmpegMuxer::getResult(nx::utils::ByteArray* const result)
{
    if (result)
    {
        result->clear(); // Should a user class do this??
        result->write(m_internalBuffer.data(), m_internalBuffer.size());
    }
    m_internalBuffer.clear();
}

QByteArray FfmpegMuxer::getSignature(QnLicensePool* licensePool, const nx::Uuid& serverId)
{
    return m_mediaSigner.buildSignature(licensePool, serverId);
}

AVCodecParameters* FfmpegMuxer::getVideoCodecParameters() const
{
    return m_videoCodecParameters;
}

AVCodecParameters* FfmpegMuxer::getAudioCodecParameters() const
{
    return m_audioCodecParameters;
}
