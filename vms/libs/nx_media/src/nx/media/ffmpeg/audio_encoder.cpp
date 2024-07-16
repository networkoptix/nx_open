// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_encoder.h"

#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/log.h>

namespace nx::media::ffmpeg {

AudioEncoder::AudioEncoder():
    m_inputFrame(av_frame_alloc()),
    m_outputPacket(av_packet_alloc())
{
}

AudioEncoder::~AudioEncoder()
{
    close();

    av_frame_free(&m_inputFrame);
    av_packet_free(&m_outputPacket);
}

void AudioEncoder::close()
{
    if (m_encoderContext)
        avcodec_free_context(&m_encoderContext);
}

CodecParametersPtr AudioEncoder::codecParams()
{
    return m_codecParams;
}

bool AudioEncoder::initialize(
    AVCodecID codecId,
    int sampleRate,
    AVSampleFormat format,
    AVChannelLayout layout,
    int bitrate)
{
    close();

    const AVCodec* codec = avcodec_find_encoder(codecId);
    if (!codec)
    {
        NX_WARNING(this, "Failed to initialize audio encoder, codec not found: %1", codecId);
        return false;
    }

    m_encoderContext = avcodec_alloc_context3(codec);
    m_encoderContext->sample_fmt = format;
    m_encoderContext->ch_layout = layout;
    m_encoderContext->sample_rate = sampleRate;
    m_encoderContext->bit_rate = bitrate;

    auto status = avcodec_open2(m_encoderContext, codec, nullptr);
    if (status < 0)
    {
        NX_WARNING(this, "Failed to open audio encoder: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return false;
    }
    m_codecParams.reset(new CodecParameters(m_encoderContext));
    return true;
}

bool AudioEncoder::sendFrame(uint8_t* data, int size)
{
    m_inputFrame->data[0] = data;
    m_inputFrame->extended_data = m_inputFrame->data;
    m_inputFrame->nb_samples = size;
    m_inputFrame->sample_rate = m_encoderContext->sample_rate;
    m_inputFrame->format = m_encoderContext->sample_fmt;
    m_inputFrame->ch_layout = m_encoderContext->ch_layout;
    auto status = avcodec_send_frame(m_encoderContext, m_inputFrame);
    if (status < 0)
    {
        NX_WARNING(this, "Could not send audio frame to encoder, Error code: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return false;
    }
    return true;
}

bool AudioEncoder::receivePacket(QnWritableCompressedAudioDataPtr& result)
{
    result.reset();
    auto status = avcodec_receive_packet(m_encoderContext, m_outputPacket);
    if (status == AVERROR(EAGAIN)) //< Not enough data to encode packet.
        return true;

    if (status < 0)
    {
        NX_WARNING(this, "Could not receive audio packet from encoder, Error code: %1",
            nx::media::ffmpeg::avErrorToString(status));
        return false;
    }

    result = std::make_shared<QnWritableCompressedAudioData>(m_outputPacket->size, m_codecParams);
    result->m_data.write((const char*)m_outputPacket->data, m_outputPacket->size);
    result->compressionType = m_encoderContext->codec_id;
    av_packet_unref(m_outputPacket);
    return true;
}

} // namespace nx::media::ffmpeg
