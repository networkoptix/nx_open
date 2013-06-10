#include "contrast_image_filter.h"

QnContrastImageFilter::QnContrastImageFilter(const ImageCorrectionParams& params):
    m_params(params),
    m_lastGamma(-1.0)
{
    memset(m_gammaCorrection, 0, sizeof(m_gammaCorrection));
}

static const __m128i  sse_0000_intrs  = _mm_setr_epi32(0x00000000, 0x00000000, 0x00000000, 0x00000000);

void QnContrastImageFilter::updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect)
{
    static const float GAMMA_EPS = 0.01f;

    if (!m_params.enabled)
        return;

    m_gamma.analizeImage(frame->data[0], frame->width, frame->height, frame->linesize[0], m_params, updateRect);

    if (qAbs(m_gamma.gamma - m_lastGamma) > GAMMA_EPS && m_gamma.gamma != 1.0)
    {
        // update gamma table
        for (int i = 0; i < 256; ++i)
            m_gammaCorrection[i] = quint8(qMin(255.0, pow(i / 255.0f, m_gamma.gamma) * 255.0));
        m_lastGamma = m_gamma.gamma;
    }

    int left = qPower2Floor(updateRect.left() * frame->width, 16);
    int right = qPower2Floor(updateRect.right() * frame->width, 16);
    int top = updateRect.top() * frame->height;
    int bottom = updateRect.bottom() * frame->height;

    int xSteps = (right - left) / 16;

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

                y = _mm_packus_epi16(_mm_mulhi_epu16(y0, aFactorIntr), _mm_mulhi_epu16(y1, aFactorIntr));
                
                quint32 tmpData = _mm_extract_epi32(y, 0);
                quint32 newVal = m_gammaCorrection[(quint8)tmpData] + 
                                 (m_gammaCorrection[(quint8)(tmpData>>8)] << 8)+
                                 (m_gammaCorrection[(quint8)(tmpData>>16)] << 16)+
                                 (m_gammaCorrection[(quint8)(tmpData>>24)] << 24);


                y = _mm_insert_epi32(y, newVal, 0);

                tmpData = _mm_extract_epi32(y, 1);
                newVal = m_gammaCorrection[(quint8)tmpData] + 
                    (m_gammaCorrection[(quint8)(tmpData>>8)] << 8)+
                    (m_gammaCorrection[(quint8)(tmpData>>16)] << 16)+
                    (m_gammaCorrection[(quint8)(tmpData>>24)] << 24);
                y = _mm_insert_epi32(y, newVal, 1);

                tmpData = _mm_extract_epi32(y, 2);
                newVal = m_gammaCorrection[(quint8)tmpData] + 
                    (m_gammaCorrection[(quint8)(tmpData>>8)] << 8)+
                    (m_gammaCorrection[(quint8)(tmpData>>16)] << 16)+
                    (m_gammaCorrection[(quint8)(tmpData>>24)] << 24);
                y = _mm_insert_epi32(y, newVal, 2);

                tmpData = _mm_extract_epi32(y, 3);
                newVal = m_gammaCorrection[(quint8)tmpData] + 
                    (m_gammaCorrection[(quint8)(tmpData>>8)] << 8)+
                    (m_gammaCorrection[(quint8)(tmpData>>16)] << 16)+
                    (m_gammaCorrection[(quint8)(tmpData>>24)] << 24);
                y = _mm_insert_epi32(y, newVal, 3);

                *srcY++ = y;
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
}
