// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider_base.h"

#include <nx/media/sse_helper.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

QString DesktopDataProviderBase::lastErrorStr() const
{
    return m_lastErrorStr;
}

/** Mux audio 1 and audio 2 to audio1 buffer. */
void DesktopDataProviderBase::stereoAudioMux(qint16 *a1, qint16 *a2, int lenInShort)
#if !defined(__arm__) && !defined(__aarch64__)
{
    __m128i* audio1 = (__m128i*) a1;
    __m128i* audio2 = (__m128i*) a2;
    for (int i = 0; i < lenInShort/8; ++i)
    {
        //*audio1 = _mm_avg_epu16(*audio1, *audio2);
        *audio1 = _mm_add_epi16(*audio1, *audio2); //< SSE2.
        audio1++;
        audio2++;
    }
    int rest = lenInShort % 8;
    if (rest > 0)
    {
        a1 += lenInShort - rest;
        a2 += lenInShort - rest;
        for (int i = 0; i < rest; ++i)
        {
            //*a1 = ((int)*a1 + (int)*a2) >> 1;
            *a1 = qMax(SHRT_MIN,qMin(SHRT_MAX, ((int)*a1 + (int)*a2)));
            a1++;
            a2++;
        }
    }
}
#else
{
    for ( int i = 0; i < lenInShort; ++i)
    {
        *a1 = qMax(SHRT_MIN,qMin(SHRT_MAX, ((int)*a1 + (int)*a2)));
        a1++;
        a2++;
    }
}
#endif

void DesktopDataProviderBase::monoToStereo(qint16 *dst, qint16 *src, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src;
        *dst++ = *src++;
    }
}

void DesktopDataProviderBase::monoToStereo(qint16 *dst, qint16 *src1, qint16 *src2, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src1++;
        *dst++ = *src2++;
    }
}

AVSampleFormat DesktopDataProviderBase::fromQtAudioFormat(const QAudioFormat& format) const
{
    switch (format.sampleFormat())
    {
        case QAudioFormat::UInt8:
            return AV_SAMPLE_FMT_U8;
        case QAudioFormat::Int16:
            return AV_SAMPLE_FMT_S16;
        case QAudioFormat::Int32:
            return AV_SAMPLE_FMT_S32;
        case QAudioFormat::Float:
            if (format.bytesPerSample() == 4)
                return AV_SAMPLE_FMT_FLT;
            if (format.bytesPerSample() == 8)
                return AV_SAMPLE_FMT_DBL;
            break;
        default:
            break;
    }

    return AV_SAMPLE_FMT_NONE;
}

AudioLayoutConstPtr DesktopDataProviderBase::getAudioLayout()
{
    if (!m_audioLayout)
        m_audioLayout.reset(new AudioLayout(m_audioEncoder.codecParameters()));

    return m_audioLayout;
}

int DesktopDataProviderBase::getAudioFrameSize()
{
    return m_audioEncoder.codecParameters()->getFrameSize();
}

bool DesktopDataProviderBase::initAudioDecoder(int sampleRate, AVSampleFormat format, int channels)
{
    AVChannelLayout layout;
    av_channel_layout_default(&layout, channels);
    return m_audioEncoder.open(AV_CODEC_ID_MP3, sampleRate, format, layout, /*bitrate*/0);
}

void DesktopDataProviderBase::pleaseStopSync()
{
    pleaseStop();
    wait();
    // Flush audio encoder.
    encodeAndPutAudioData(nullptr, 0, /*channelNumber*/ 0);
}

bool DesktopDataProviderBase::encodeAndPutAudioData(uint8_t* buffer, int size, int channelNumber)
{
    if (!m_audioEncoder.sendFrame(buffer, size))
        return false;

    QnWritableCompressedAudioDataPtr packet;
    while (!needToStop())
    {
        if (!m_audioEncoder.receivePacket(packet))
            return false;

        if (!packet)
            break;

        if (m_utcTimstampOffsetUs == 0)
            m_utcTimstampOffsetUs = qnSyncTime->currentUSecsSinceEpoch();

        packet->timestamp += m_utcTimstampOffsetUs;
        packet->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        packet->dataProvider = this;
        packet->channelNumber = channelNumber;
        if (dataCanBeAccepted())
            putData(packet);
    }

    return true;
}

} // namespace nx::vms::client::core
