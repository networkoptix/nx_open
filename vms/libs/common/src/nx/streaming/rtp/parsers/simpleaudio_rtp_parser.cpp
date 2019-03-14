#include "simpleaudio_rtp_parser.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QtEndian>

#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>

namespace nx::streaming::rtp {

SimpleAudioParser::SimpleAudioParser():
    AudioStreamParser(),
    m_audioLayout( new QnRtspAudioLayout() )
{
    StreamParser::setFrequency(8000);
    m_channels = 1;
    m_codecId = AV_CODEC_ID_PCM_MULAW;
    m_sampleFormat = AV_SAMPLE_FMT_S16;
    m_bits_per_coded_sample = 16;
}

void SimpleAudioParser::setCodecId(AVCodecID codecId)
{
    m_codecId = codecId;
}

void SimpleAudioParser::setSdpInfo(const Sdp::Media& sdp)
{
    // determine here:
    // 1. sizeLength(au size in bits)  or constantSize

    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
    if (sdp.rtpmap.channels > 0)
        m_channels = sdp.rtpmap.channels;

    const auto context = new QnAvCodecMediaContext(m_codecId);
    m_context = QnConstMediaContextPtr(context);
    const auto av = context->getAvCodecContext();
    av->channels = m_channels;
    av->sample_rate = StreamParser::getFrequency();
    av->sample_fmt = m_sampleFormat;
    av->time_base.num = 1;
    av->bits_per_coded_sample = m_bits_per_coded_sample;
    av->time_base.den = StreamParser::getFrequency();

    QnResourceAudioLayout::AudioTrack track;
    track.codecContext = m_context;
    m_audioLayout->setAudioTrackInfo(track);
}

bool SimpleAudioParser::processData(quint8* rtpBufferBase, int bufferOffset, int bufferSize, bool& gotData)
{
    gotData = false;
    const quint8* rtpBuffer = rtpBufferBase + bufferOffset;

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    const quint8* curPtr = rtpBuffer + RtpHeader::kSize;

    if (rtpHeader->extension)
    {
        if (bufferSize < RtpHeader::kSize + 4)
            return false;

        int extWords = ((int(curPtr[2]) << 8) + curPtr[3]);
        curPtr += extWords*4 + 4;
    }
    const quint8* end = rtpBuffer + bufferSize;
    if (curPtr >= end)
        return false;
    if (rtpHeader->padding)
        end -= end[-1];
    if (curPtr >= end)
        return false;


    QnWritableCompressedAudioDataPtr audioData = QnWritableCompressedAudioDataPtr(new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, end - curPtr));
    audioData->compressionType = !m_context? AV_CODEC_ID_NONE : m_context->getCodecId();
    audioData->context = m_context;
    audioData->timestamp = qFromBigEndian(rtpHeader->timestamp);
    audioData->m_data.write((const char*)curPtr, end - curPtr);
    m_audioData.push_back(audioData);
    gotData = true;
    return true;
}

QnConstResourceAudioLayoutPtr SimpleAudioParser::getAudioLayout()
{
    return m_audioLayout;
}

void SimpleAudioParser::setBitsPerSample(int value)
{
    m_bits_per_coded_sample = value;
}

void SimpleAudioParser::setSampleFormat(AVSampleFormat sampleFormat)
{
    m_sampleFormat = sampleFormat;
}

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS
