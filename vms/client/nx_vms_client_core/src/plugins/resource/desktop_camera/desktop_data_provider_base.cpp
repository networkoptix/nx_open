// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider_base.h"

QnDesktopDataProviderBase::QnDesktopDataProviderBase(const QnResourcePtr& ptr) :
    QnAbstractMediaStreamDataProvider(ptr)
{

}

QnDesktopDataProviderBase::~QnDesktopDataProviderBase()
{

}

QString QnDesktopDataProviderBase::lastErrorStr() const
{
    return m_lastErrorStr;
}

/** Mux audio 1 and audio 2 to audio1 buffer. */
void QnDesktopDataProviderBase::stereoAudioMux(qint16 *a1, qint16 *a2, int lenInShort)
#if !defined(__arm__) && !defined(__aarch64__)
{
    // Used intrisicts for SSE. It is portable for MSVC, GCC (mac, linux), Intel compiler.
    
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

void QnDesktopDataProviderBase::monoToStereo(qint16 *dst, qint16 *src, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src;
        *dst++ = *src++;
    }
}

void QnDesktopDataProviderBase::monoToStereo(qint16 *dst, qint16 *src1, qint16 *src2, int lenInShort)
{
    for (int i = 0; i < lenInShort; ++i)
    {
        *dst++ = *src1++;
        *dst++ = *src2++;
    }
}

AVSampleFormat QnDesktopDataProviderBase::fromQtAudioFormat(const QAudioFormat& format) const
{
    if (format.sampleType() == QAudioFormat::SampleType::SignedInt)
    {
        if (format.sampleSize() == 16)
            return AV_SAMPLE_FMT_S16;

        if (format.sampleSize() == 32)
            return AV_SAMPLE_FMT_S32;
    }
    else if (format.sampleType() == QAudioFormat::SampleType::UnSignedInt)
    {
        if (format.sampleSize() == 8)
            return AV_SAMPLE_FMT_U8;
    }
    else if (format.sampleType() == QAudioFormat::SampleType::Float)
    {
        if (format.sampleSize() == 32)
            return AV_SAMPLE_FMT_FLT;

        if (format.sampleSize() == 64)
            return AV_SAMPLE_FMT_DBL;
    }

    return AV_SAMPLE_FMT_NONE;
}
