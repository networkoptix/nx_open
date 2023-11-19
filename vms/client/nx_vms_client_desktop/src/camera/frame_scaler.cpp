// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "frame_scaler.h"

#include <nx/media/sse_helper.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/math/math.h>

extern "C" {
#include <libavutil/pixdesc.h>
} // extern "C"

namespace {

#if defined(NX_SSE2_SUPPORTED)
const __m128i  sse_00ffw_intrs = _mm_setr_epi32(0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff);
const __m128i  sse_000000ffw_intrs = _mm_setr_epi32(0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff);

void downscalePlate_factor2_sse2_intr(
    unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
    const unsigned int width, const unsigned int src_stride, unsigned int height, int fillter)
{
    NX_ASSERT(qPower2Ceil(width, 16) <= src_stride && qPower2Ceil(width/2,16) <= dst_stride);

    const __m128i color_const_intrs = _mm_setr_epi16(fillter, fillter, fillter, fillter, fillter, fillter, fillter, fillter); /* SSE2. */
    int xSteps = qPower2Ceil(width, 16) / 16;
    const unsigned char* src_line1 = src;
    const unsigned char* src_line2 = src_line1 + src_stride;

    for (int y = height/2; y > 0; --y)
    {
        const __m128i *src1 = (const __m128i*) src_line1;
        const __m128i *src2 = (const __m128i*) src_line2;
        __m128i* dstIntr = (__m128i*) dst;

        for (int x = xSteps/2; x > 0; --x)
        {
            _mm_prefetch((const char*) (src1+2), _MM_HINT_NTA); /* SSE. */
            _mm_prefetch((const char*) (src2+2), _MM_HINT_NTA); /* SSE. */

            __m128i s00 = _mm_and_si128(_mm_loadu_si128(src1), sse_00ffw_intrs); /* SSE2. */
            __m128i s01 = _mm_srli_epi16(_mm_loadu_si128(src1), 8); /* SSE2. */
            __m128i s02 = _mm_and_si128(_mm_loadu_si128(src1+1), sse_00ffw_intrs); /* SSE2. */
            __m128i s03 = _mm_srli_epi16(_mm_loadu_si128(src1+1), 8); /* SSE2. */

            __m128i s10 = _mm_and_si128(_mm_loadu_si128(src2), sse_00ffw_intrs); /* SSE2. */
            __m128i s11 = _mm_srli_epi16(_mm_loadu_si128(src2), 8); /* SSE2. */
            __m128i s12 = _mm_and_si128(_mm_loadu_si128(src2+1), sse_00ffw_intrs); /* SSE2. */
            __m128i s13 = _mm_srli_epi16(_mm_loadu_si128(src2+1), 8); /* SSE2. */

            __m128i rez1 = _mm_avg_epu16(_mm_avg_epu16(s00, s01), _mm_avg_epu16(s10, s11)); /* SSE2. */
            __m128i rez2 = _mm_avg_epu16(_mm_avg_epu16(s02, s03), _mm_avg_epu16(s12, s13)); /* SSE2. */
            *dstIntr++ = _mm_packus_epi16(rez1, rez2); /* SSE2. */
            src1 += 2;
            src2 += 2;
        }
        if (xSteps & 1)
        {
            __m128i s00 = _mm_and_si128(_mm_loadu_si128(src1), sse_00ffw_intrs); /* SSE2. */
            __m128i s01 = _mm_srli_epi16(_mm_loadu_si128(src1), 8); /* SSE2. */
            __m128i s10 = _mm_and_si128(_mm_loadu_si128(src2), sse_00ffw_intrs); /* SSE2. */
            __m128i s11 = _mm_srli_epi16(_mm_loadu_si128(src2), 8); /* SSE2. */

            __m128i rez1 = _mm_avg_epu16(_mm_avg_epu16(s00, s01), _mm_avg_epu16(s10, s11)); /* SSE2. */
            *dstIntr = _mm_packus_epi16(rez1, color_const_intrs); /* SSE2. */
        }
        dst += dst_stride;
        src_line1 += src_stride*2;
        src_line2 += src_stride*2;
    }
}
#else // defined(NX_SSE2_SUPPORTED)

void downscalePlate_factor2_sse2_intr(
    unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
    const unsigned int width, const unsigned int src_stride, unsigned int height, int fillter)
{
    NX_CRITICAL(false);
}

#endif // defined(NX_SSE2_SUPPORTED)

#if !defined(__arm__) && !defined(__aarch64__)
void sse4_attribute downscalePlate_factor4_ssse3_intr(
    unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
    const unsigned int width, const unsigned int src_stride, unsigned int height, int filler)
{
    NX_ASSERT(qPower2Ceil(width, 16) <= src_stride && qPower2Ceil(width/4,8) <= dst_stride);

    const __m128i color_const_intrs = _mm_setr_epi16(filler, filler, filler, filler, filler, filler, filler, filler); /* SSE2. */
    int xSteps = qPower2Ceil(width, 16) / 16;
    const unsigned char* src_line1 = src;
    const unsigned char* src_line2 = src_line1 + src_stride*3;

    for (int y = height/4; y > 0; --y)
    {
        const __m128i *src1 = (const __m128i*) src_line1;
        const __m128i *src2 = (const __m128i*) src_line2;
        quint8* dstCur = dst;

        for (int x = xSteps/2; x > 0; --x)
        {
            _mm_prefetch((const char*) (src1+2), _MM_HINT_NTA); /* SSE. */
            _mm_prefetch((const char*) (src2+2), _MM_HINT_NTA); /* SSE. */

            __m128i s00 = _mm_and_si128(_mm_loadu_si128(src1), sse_00ffw_intrs); /* SSE2. */
            __m128i s02 = _mm_and_si128(_mm_loadu_si128(src1+1), sse_00ffw_intrs); /* SSE2. */
            __m128i s10 = _mm_and_si128(_mm_loadu_si128(src2), sse_00ffw_intrs); /* SSE2. */
            __m128i s12 = _mm_and_si128(_mm_loadu_si128(src2+1), sse_00ffw_intrs); /* SSE2. */

            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16_ssse3(s00, s02), _mm_hadd_epi16_ssse3(s10, s12)),1); /* SSSE3. */

            _mm_storel_epi64((__m128i*) dstCur, _mm_packus_epi16(rez1, rez1)); /* SSE2. */
            src1 += 2;
            src2 += 2;
            dstCur += 8;
        }
        if (xSteps & 1)
        {
            __m128i s00 = _mm_and_si128(_mm_loadu_si128(src1), sse_00ffw_intrs); /* SSE2. */
            __m128i s10 = _mm_and_si128(_mm_loadu_si128(src2), sse_00ffw_intrs); /* SSE2. */

            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16_ssse3(s00, color_const_intrs), _mm_hadd_epi16_ssse3(s10, color_const_intrs)),1); /* SSSE3. */
            _mm_storel_epi64((__m128i*) dstCur, _mm_packus_epi16(rez1, rez1)); /* SSE2. */
        }
        dst += dst_stride;
        src_line1 += src_stride*4;
        src_line2 += src_stride*4;
    }
}

void downscalePlate_factor8_sse41_intr(
    unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
    const unsigned int width, const unsigned int src_stride, unsigned int height, int filler)
{
    NX_ASSERT(qPower2Ceil(width, 16) <= src_stride && qPower2Ceil(width/8,4) <= dst_stride);

    const __m128i color_const_intrs = _mm_setr_epi16(filler, filler, filler, filler, filler, filler, filler, filler); /* SSE2. */
    int xSteps = qPower2Ceil(width, 16) / 16;
    const unsigned char* src_line1 = src;
    const unsigned char* src_line2 = src_line1 + src_stride*7;

    for (int y = height/8; y > 0; --y)
    {
        const __m128i *src1 = (const __m128i*) src_line1;
        const __m128i *src2 = (const __m128i*) src_line2;
        quint8* dstCur = dst;


        for (int x = xSteps/2; x > 0; --x)
        {
            _mm_prefetch((const char*) (src1+2), _MM_HINT_NTA); /* SSE. */
            _mm_prefetch((const char*) (src2+2), _MM_HINT_NTA); /* SSE. */

            __m128i s00 = _mm_and_si128(_mm_loadu_si128(src1), sse_000000ffw_intrs); /* SSE2. */
            __m128i s02 = _mm_and_si128(_mm_loadu_si128(src1+1), sse_000000ffw_intrs); /* SSE2. */
            __m128i s10 = _mm_and_si128(_mm_loadu_si128(src2), sse_000000ffw_intrs); /* SSE2. */
            __m128i s12 = _mm_and_si128(_mm_loadu_si128(src2+1), sse_000000ffw_intrs); /* SSE2. */

            s00 = _mm_packus_epi32_sse4(s00, s02); /* SSE4. */
            s10 = _mm_packus_epi32_sse4(s10, s12); /* SSE4. */
            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16_ssse3(s00, s00), _mm_hadd_epi16_ssse3(s10, s10)),1); /* SSSE3. */

            __m128i tmp = _mm_packus_epi16(rez1, rez1); /* SSE2. */
            _mm_store_ss((float*) dstCur, *(__m128*)(&tmp)); /* SSE. */
            src1 += 2;
            src2 += 2;
            dstCur += 4;
        }
        if (xSteps & 1)
        {
            __m128i s00 = _mm_and_si128(_mm_loadu_si128(src1), sse_00ffw_intrs); /* SSE2. */
            __m128i s10 = _mm_and_si128(_mm_loadu_si128(src2), sse_00ffw_intrs); /* SSE2. */
            s00 = _mm_packus_epi32_sse4(s00, color_const_intrs); /* SSE4. */
            s10 = _mm_packus_epi32_sse4(s10, color_const_intrs); /* SSE4. */

            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16_ssse3(s00, color_const_intrs), _mm_hadd_epi16_ssse3(s10, color_const_intrs)),1); /* SSSE3. */
            __m128i tmp = _mm_packus_epi16(rez1, color_const_intrs); /* SSE2. */
            _mm_store_ss((float*) dstCur, *(__m128*)(&tmp)); /* SSE. */
        }
        dst += dst_stride;
        src_line1 += src_stride*8;
        src_line2 += src_stride*8;
    }
}

#else // !defined(__arm__) && !defined(__aarch64__)

void downscalePlate_factor4_ssse3_intr(
    unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
    const unsigned int width, const unsigned int src_stride, unsigned int height, int filler)
{
    NX_CRITICAL(false);
}

void downscalePlate_factor8_sse41_intr(
    unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
    const unsigned int width, const unsigned int src_stride, unsigned int height, int filler)
{
    NX_CRITICAL(false);
}

#endif // !defined(__arm__) && !defined(__aarch64__)

template <auto scalerFunc>
void downscalePlanes(
    const AVPixFmtDescriptor* descriptor,
    const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst,
    const int croppedWidth)
{
    for (int i = 0; i < QnFfmpegHelper::planeCount(descriptor) && src->data[i] && dst->data[i]; ++i)
    {
        int width = croppedWidth;
        int height = src->height;
        quint8 fillerColor = 0x00;
        if (QnFfmpegHelper::isChromaPlane(i, descriptor))
        {
            width >>= descriptor->log2_chroma_w;
            height >>= descriptor->log2_chroma_h;
            fillerColor = 0x80;
        }
        scalerFunc(dst->data[i], dst->linesize[i], src->data[i], width, src->linesize[i], height, fillerColor);
    }
}

} // namespace

void QnFrameScaler::downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, DownscaleFactor factor)
{
    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get((AVPixelFormat)src->format);
    if (!descriptor)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported pixel format: %1", dst->format);
        return;
    }

    // after downscale chroma_width must be divisible by 4 ( opengl requirements )
    const int mod_w = (1 << descriptor->log2_chroma_w) * factor * 4;
    const int croppedWidth = qPower2Floor(src->width, mod_w);

    const int scaledWidth = croppedWidth / factor;
    const int scaledHeight = src->height / factor;

    if (scaledWidth != dst->width || scaledHeight != dst->height || src->format != dst->format || dst->isExternalData())
    {
        if (!dst->reallocate(scaledWidth, scaledHeight, src->format))
        {
            NX_WARNING(NX_SCOPE_TAG,
                "Can't allocate image. Size: %1x%2, format: %3",
                scaledWidth, scaledHeight, src->format);
            return;
        }
    }

    switch(factor)
    {
        case factor_2:
            if (useSSE2())
                downscalePlanes<downscalePlate_factor2_sse2_intr>(descriptor, src, dst, croppedWidth);
            else
                downscalePlanes<downscalePlate_factor2>(descriptor, src, dst, croppedWidth);
            break;
        case factor_4:
            if (useSSSE3())
                downscalePlanes<downscalePlate_factor4_ssse3_intr>(descriptor, src, dst, croppedWidth);
            else
                downscalePlanes<downscalePlate_factor4>(descriptor, src, dst, croppedWidth);
            break;
        case factor_8:
            if (useSSE41())
                downscalePlanes<downscalePlate_factor8_sse41_intr>(descriptor, src, dst, croppedWidth);
            else
                downscalePlanes<downscalePlate_factor8>(descriptor, src, dst, croppedWidth);
            break;
        default:
            NX_ASSERT(false, "Unsupported scale factor");
    }
}

void QnFrameScaler::downscalePlate_factor2(
    unsigned char* dst, int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height, quint8 /*filler*/)
{
    const unsigned char* src_line1 = src;
    const unsigned char* src_line2 = src + src_stride;

    for (int y = 0; y < src_height/2; ++y)
    {
        for (int x = 0; x < src_width/2; ++x)
        {
            *dst = ( (unsigned int)*src_line1 + *(src_line1+1) + (unsigned int)*src_line2 + *(src_line2+1) + 2 ) >> 2;
            src_line1+=2;
            src_line2+=2;

            ++dst;
        }
        dst += dstStride - src_width/2;

        src_line1-= src_width;
        src_line1+=(src_stride<<1);

        src_line2-= src_width;
        src_line2+=(src_stride<<1);
    }
}

void QnFrameScaler::downscalePlate_factor4(
    unsigned char* dst,  int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height, quint8 /*filler*/)
{
    const unsigned char* src_line1 = src;
    const unsigned char* src_line2 = src + 3*src_stride;

    for (int y = 0; y < src_height/4; ++y)
    {
        for (int x = 0; x < src_width/4; ++x)
        {
            *dst = ( (unsigned int)*src_line1 + *(src_line1+3) + (unsigned int)*src_line2 + *(src_line2+3) + 2 ) >> 2;
            src_line1+=4;
            src_line2+=4;

            ++dst;
        }
        dst += dstStride - src_width/4;

        src_line1-= src_width;
        src_line1+=(src_stride<<2);

        src_line2-= src_width;
        src_line2+=(src_stride<<2);
    }
}


void QnFrameScaler::downscalePlate_factor8(
    unsigned char* dst,  int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height, quint8 /*filler*/)
{
    const unsigned char* src_line1 = src;
    const unsigned char* src_line2 = src + 7*src_stride;

    for (int y = 0; y < src_height/8; ++y)
    {
        for (int x = 0; x < src_width/8; ++x)
        {
            *dst = ( (unsigned int)*src_line1 + *(src_line1+7) + (unsigned int)*src_line2 + *(src_line2+7) + 2 ) >> 2;
            src_line1+=8;
            src_line2+=8;

            ++dst;
        }
        dst += dstStride - src_width/8;

        src_line1-= src_width;
        src_line1+=(src_stride<<3);

        src_line2-= src_width;
        src_line2+=(src_stride<<3);
    }
}
