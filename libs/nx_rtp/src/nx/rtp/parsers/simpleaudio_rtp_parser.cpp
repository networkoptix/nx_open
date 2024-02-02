// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "simpleaudio_rtp_parser.h"

#include <QtCore/QtEndian>

#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/rtp/rtp.h>

namespace nx::rtp {

SimpleAudioParser::SimpleAudioParser(AVCodecID codecId):
    AudioStreamParser(),
    m_codecId(codecId)
{
    StreamParser::setFrequency(8000);
    m_bits_per_coded_sample = av_get_exact_bits_per_sample(codecId);
}

void SimpleAudioParser::setSdpInfo(const Sdp::Media& sdp)
{
    // determine here:
    // 1. sizeLength(au size in bits)  or constantSize

    if (sdp.rtpmap.clockRate > 0)
    {
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
        m_bitrate = m_bits_per_coded_sample * sdp.rtpmap.clockRate;
    }
    if (sdp.rtpmap.channels > 0)
        m_channels = sdp.rtpmap.channels;

    m_context = std::make_shared<CodecParameters>();
    const auto codecParams = m_context->getAvCodecParameters();
    codecParams->codec_type = AVMEDIA_TYPE_AUDIO;
    codecParams->codec_id = m_codecId;
    codecParams->channels = m_channels;
    codecParams->sample_rate = StreamParser::getFrequency();
    codecParams->format = AV_SAMPLE_FMT_S16;
    codecParams->bits_per_coded_sample = m_bits_per_coded_sample;
    codecParams->bit_rate = m_bitrate;
}

Result SimpleAudioParser::processData(
    const RtpHeader& rtpHeader,
    quint8* rtpBufferBase,
    int bufferOffset,
    int bufferSize,
    bool& gotData)
{
    gotData = false;
    const quint8* rtpBuffer = rtpBufferBase + bufferOffset;
    const quint8* curPtr = rtpBuffer;
    const quint8* end = rtpBuffer + bufferSize;

    if (curPtr >= end)
        return {false, "Malformed audio packet"};

    QnWritableCompressedAudioDataPtr audioData = QnWritableCompressedAudioDataPtr(
        new QnWritableCompressedAudioData(end - curPtr));
    audioData->compressionType = !m_context? AV_CODEC_ID_NONE : m_context->getCodecId();
    audioData->context = m_context;
    audioData->timestamp = rtpHeader.getTimestamp();
    audioData->m_data.write((const char*)curPtr, end - curPtr);
    m_audioData.push_back(audioData);
    gotData = true;
    return {true};
}

CodecParametersConstPtr SimpleAudioParser::getCodecParameters()
{
    return m_context;
}

void SimpleAudioParser::setBitsPerSample(int value)
{
    m_bits_per_coded_sample = value;
    m_bitrate = m_bits_per_coded_sample * getFrequency();
}

} // namespace nx::rtp
