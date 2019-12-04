#include "ffmpeg_audio_demuxer.h"

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

} // extern "C"

#include <utils/media/ffmpeg_helper.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/audio_data_packet.h>
#include <utils/common/synctime.h>

namespace nx::vms::server::http_audio {

int64_t UtcTimestamp::getTimestampUs(int64_t timestamp, const AVRational& timescale)
{
    if (m_firstTimestamp == -1)
    {
        m_firstTimestamp = timestamp;
        m_utcStartTime = qnSyncTime->currentUSecsSinceEpoch();
        return m_utcStartTime;
    }
    int64_t pts = av_rescale_q(timestamp - m_firstTimestamp, timescale, m_usecTimescale);
    return pts + m_utcStartTime;
}

FfmpegAudioDemuxer::~FfmpegAudioDemuxer()
{
    close();
}

void FfmpegAudioDemuxer::close()
{
    if (m_formatContext)
    {
        m_formatContext->pb = nullptr;
        avformat_close_input(&m_formatContext);
    }
}

bool FfmpegAudioDemuxer::open(
    FfmpegIoContextPtr ioContext, const std::optional<StreamConfig>& config)
{
    close();

    m_ioContext = std::move(ioContext);
    if (m_formatContext)
    {
        m_formatContext->pb = nullptr;
        avformat_close_input(&m_formatContext);
    }

    m_formatContext = avformat_alloc_context();
    m_formatContext->pb = m_ioContext->getAvioContext();

    int status;
    if (config)
    {
        NX_DEBUG(this, "Open ffmpeg demuxer with params: %1 %2 %3",
            config->format, config->sampleRate, config.value().channelsNumber);
        AVDictionary *options = NULL;
        av_dict_set(&options, "sample_rate", config.value().sampleRate.c_str(), 0);
        av_dict_set(&options, "channels", config.value().channelsNumber.c_str(), 0);
        status = avformat_open_input(
            &m_formatContext, "", av_find_input_format(config.value().format.c_str()), &options);
    }
    else
    {
        NX_DEBUG(this, "Open ffmpeg demuxer");
        status = avformat_open_input(&m_formatContext, "", 0, 0);
    }

    if (status < 0)
    {
        NX_ERROR(this, "Failed to open audio stream, error: %1",
            QnFfmpegHelper::getErrorStr(status));
        return false;
    }
    return true;
}

QnAbstractMediaDataPtr FfmpegAudioDemuxer::getNextData()
{
    if (!m_formatContext)
    {
        NX_ERROR(this, "Invalid usage of uninitilized demuxer");
        return nullptr;
    }

    QnFfmpegAvPacket packet;
    QnAbstractMediaDataPtr data;
    AVStream *stream;
    int status = av_read_frame(m_formatContext, &packet);
    if (status < 0)
    {
        if (status != AVERROR_EOF)
        {
            NX_ERROR(this, "Failed to read frame, error: %1",
                QnFfmpegHelper::getErrorStr(status));
        }
        return nullptr;
    }

    stream = m_formatContext->streams[packet.stream_index];
    if (stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
        return QnAbstractMediaDataPtr(new QnEmptyMediaData());

    stream->codec->frame_size = stream->codecpar->frame_size;
    stream->codec->channel_layout = stream->codecpar->channel_layout;
    stream->codec->channels = stream->codecpar->channels;
    stream->codec->sample_rate = stream->codecpar->sample_rate;
    QnConstMediaContextPtr codecContext(new QnAvCodecMediaContext(stream->codec));
    QnWritableCompressedAudioData* audioData = new QnWritableCompressedAudioData(
        CL_MEDIA_ALIGNMENT, packet.size, codecContext);
    audioData->duration = av_rescale_q(packet.duration, stream->time_base, AVRational{1, 1000000});
    data = QnAbstractMediaDataPtr(audioData);
    audioData->channelNumber = 0;
    audioData->timestamp = m_utcTimestamp.getTimestampUs(packet.pts, stream->time_base);
    audioData->m_data.write((const char*) packet.data, packet.size);
    data->compressionType = stream->codecpar->codec_id;
    data->flags = static_cast<QnAbstractMediaData::MediaFlags>(packet.flags);
    data->flags |= QnAbstractMediaData::MediaFlag::MediaFlags_AVKey;
    return data;
}

} // namespace nx::vms::server::http_audio
