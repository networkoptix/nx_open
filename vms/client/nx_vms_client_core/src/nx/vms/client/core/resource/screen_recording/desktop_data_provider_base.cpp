// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider_base.h"

namespace nx::vms::client::core {

DesktopDataProviderBase::DesktopDataProviderBase(const QnResourcePtr& ptr):
    QnAbstractMediaStreamDataProvider(ptr)
{

}

DesktopDataProviderBase::~DesktopDataProviderBase()
{

}

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

} // namespace nx::vms::client::core
