// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "contrast_image_filter.h"

#include <nx/media/ffmpeg/frame_info.h>
#include <nx/media/sse_helper.h>
#include <nx/utils/math/math.h>

QnContrastImageFilter::QnContrastImageFilter(const nx::vms::api::ImageCorrectionData& params):
    m_params(params),
    m_lastGamma(-1.0)
{
    memset(m_gammaCorrection, 0, sizeof(m_gammaCorrection));
}

bool QnContrastImageFilter::isFormatSupported(CLVideoDecoderOutput* frame) const
{
    return frame->data[1]; //< if several video planes are present, format is supported
}

#if defined(NX_SSE2_SUPPORTED)
    static const __m128i sse_0000_intrs =
        _mm_setr_epi32(0x00000000, 0x00000000, 0x00000000, 0x00000000);
#elif defined(__arm__) && defined(__ARM_NEON)
    // TODO: ARM.
#else
    // TODO: C fallback routine.
#endif

CLVideoDecoderOutputPtr QnContrastImageFilter::updateImage(
    const CLVideoDecoderOutputPtr& srcFrame,
    const QnAbstractCompressedMetadataPtr&)
{
    const CLVideoDecoderOutputPtr& frame = srcFrame;

    static const float GAMMA_EPS = 0.01f;

    if (!m_params.enabled)
        return frame;
    if (!isFormatSupported(frame.data()))
        return frame;

    m_gamma.analyseImage(frame->data[0], frame->width, frame->height, frame->linesize[0], m_params);

    if (qAbs(m_gamma.gamma - m_lastGamma) > GAMMA_EPS && m_gamma.gamma != 1.0)
    {
        // update gamma table
        for (int i = 0; i < 256; ++i)
            m_gammaCorrection[i] = quint8(qMin(255.0, pow(i / 255.0f, m_gamma.gamma) * 255.0));
        m_lastGamma = m_gamma.gamma;
    }

    int left = 0;
    int right = frame->width;
    int top = 0;
    int bottom = frame->height;

    int xSteps = (right - left) / 16;

    #if defined(NX_SSE2_SUPPORTED)
        quint16 aCoeff = quint16(m_gamma.aCoeff * 256.0f + 0.5);
        __m128i  aFactorIntr  = _mm_setr_epi16(aCoeff, aCoeff, aCoeff, aCoeff, aCoeff, aCoeff, aCoeff, aCoeff);

        quint8 bCoeff = quint8(qAbs(m_gamma.bCoeff) * 256.0 + 0.5);
        __m128i  bFactorIntr  = _mm_setr_epi8(bCoeff, bCoeff, bCoeff, bCoeff, bCoeff, bCoeff, bCoeff, bCoeff,
                                              bCoeff, bCoeff, bCoeff, bCoeff, bCoeff, bCoeff, bCoeff, bCoeff);

        __m128i *srcY =    (__m128i*)  (frame->data[0] + top * frame->linesize[0] + left);
        __m128i *srcYEnd = (__m128i*)  (frame->data[0] + bottom * frame->linesize[0] + left);
        int strideCoeff = frame->linesize[0]/16 - xSteps;
        if (m_gamma.gamma != 1.0)
        {
            while (srcY < srcYEnd)
            {
                for (int x = xSteps; x > 0; --x)
                {
                    __m128i y = _mm_subs_epu8(*srcY, bFactorIntr);
                    __m128i y0 = _mm_unpacklo_epi8(sse_0000_intrs, y);
                    __m128i y1 = _mm_unpackhi_epi8(sse_0000_intrs, y);

                    typedef union
                    {
                        __m128i v;
                        uint32_t a[4];
                        uint8_t b[16];
                    } U32;
                    U32 tmp;

                    tmp.v = _mm_packus_epi16(_mm_mulhi_epu16(y0, aFactorIntr), _mm_mulhi_epu16(y1, aFactorIntr));

                    tmp.a[0] = m_gammaCorrection[(quint8)tmp.a[0]] +
                                     (m_gammaCorrection[(quint8)(tmp.a[0]>>8)] << 8)+
                                     (m_gammaCorrection[(quint8)(tmp.a[0]>>16)] << 16)+
                                     (m_gammaCorrection[(quint8)(tmp.a[0]>>24)] << 24);

                    tmp.a[1] = (quint32) m_gammaCorrection[tmp.b[4]] +
                        (m_gammaCorrection[tmp.b[5]] << 8) +
                        (m_gammaCorrection[tmp.b[6]] << 16) +
                        (m_gammaCorrection[tmp.b[7]] << 24);

                    tmp.a[2] = (quint32) m_gammaCorrection[tmp.b[8]] +
                        (m_gammaCorrection[tmp.b[9]] << 8) +
                        (m_gammaCorrection[tmp.b[10]] << 16) +
                        (m_gammaCorrection[tmp.b[11]] << 24);

                    tmp.a[3] = (quint32) m_gammaCorrection[tmp.b[12]] +
                        (m_gammaCorrection[tmp.b[13]] << 8) +
                        (m_gammaCorrection[tmp.b[14]] << 16) +
                        (m_gammaCorrection[tmp.b[15]] << 24);

                    *srcY++ = tmp.v;
                }
                srcY += strideCoeff;
            }
        }
        else {
            while (srcY < srcYEnd)
            {
                for (int x = xSteps; x > 0; --x)
                {
                    __m128i y = _mm_subs_epu8(*srcY, bFactorIntr);
                    __m128i y0 = _mm_unpacklo_epi8(sse_0000_intrs, y);
                    __m128i y1 = _mm_unpackhi_epi8(sse_0000_intrs, y);

                    *srcY++ = _mm_packus_epi16(_mm_mulhi_epu16(y0, aFactorIntr), _mm_mulhi_epu16(y1, aFactorIntr));
                }
                srcY += strideCoeff;
            }
        }
    #elif defined(__arm__) && defined(__ARM_NEON)
        // TODO: ARM.
    #else
        // TODO: C fallback routine.
    #endif

    return frame;
}
