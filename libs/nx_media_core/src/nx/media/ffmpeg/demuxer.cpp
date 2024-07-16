// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "demuxer.h"

#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/log.h>

namespace nx::media::ffmpeg {

Demuxer::~Demuxer()
{
    close();
}

void Demuxer::close()
{
    if (m_formatContext)
    {
        m_formatContext->pb = nullptr;
        avformat_close_input(&m_formatContext);
    }
}

bool Demuxer::open(IoContextPtr ioContext)
{
    close();
    m_ioContext = std::move(ioContext);
    m_formatContext = avformat_alloc_context();
    m_formatContext->pb = m_ioContext->getAvioContext();

    int status = 0;
    status = avformat_open_input(&m_formatContext, "", 0, 0);

    if (status < 0)
    {
        NX_ERROR(this, "Failed to open audio stream, error: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return false;
    }

    status = avformat_find_stream_info(m_formatContext, nullptr);
    if (status < 0)
    {
        NX_ERROR(this, "Failed to find stream info, error: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return false;
    }
    return true;
}

QnAbstractMediaDataPtr Demuxer::getNextData()
{
    if (!m_formatContext)
    {
        NX_ERROR(this, "Invalid usage of uninitilized demuxer");
        return nullptr;
    }

    QnFfmpegAvPacket packet;
    int status = av_read_frame(m_formatContext, &packet);
    if (status < 0)
    {
        if (status != AVERROR_EOF)
        {
            NX_ERROR(this, "Failed to read frame, error: %1",
                nx::media::ffmpeg::avErrorToString(status));
        }
        return nullptr;
    }

    AVStream* const stream = m_formatContext->streams[packet.stream_index];

    if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
        return QnAbstractMediaDataPtr(new QnEmptyMediaData());

    CodecParametersConstPtr codecContext = std::make_shared<CodecParameters>(stream->codecpar);
    auto data = std::make_shared<QnWritableCompressedVideoData>(packet.size, codecContext);
    data->channelNumber = 0;
    data->timestamp = packet.pts;
    data->m_data.write((const char*) packet.data, packet.size);
    data->compressionType = stream->codecpar->codec_id;
    data->flags |= QnAbstractMediaData::MediaFlag::MediaFlags_AVKey;
    return data;
}

} // namespace nx::media::ffmpeg
