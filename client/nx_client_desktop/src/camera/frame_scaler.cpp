#include "frame_scaler.h"

#include "utils/media/sse_helper.h"
#include "utils/math/math.h"

#if !defined(__arm__) && !defined(__aarch64__)

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

#else // !defined(__arm__)

void downscalePlate_factor2_sse2_intr(
    unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
    const unsigned int width, const unsigned int src_stride, unsigned int height, int fillter)
{
    NX_CRITICAL(false);
}

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

#endif // !defined(__arm__)

void QnFrameScaler::downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, DownscaleFactor factor)
{
    int src_width = src->width;
    int src_height = src->height;

    const int chroma_h_factor = (src->format == AV_PIX_FMT_YUV420P || src->format == AV_PIX_FMT_YUV422P) ? 2 : 1;
    const int chroma_v_factor = (src->format == AV_PIX_FMT_YUV420P) ? 2 : 1;

    // after downscale chroma_width must be divisible by 4 ( opengl requirements )
    const int mod_w = chroma_h_factor*factor*4;
    if ((src_width%mod_w) != 0)
        src_width = src_width/mod_w*mod_w;

    int scaledWidth = src_width/factor;
    int scaledHeight = src_height/factor;

    if (scaledWidth != dst->width || scaledHeight != dst->height || src->format != dst->format || dst->isExternalData())
        dst->reallocate(scaledWidth, scaledHeight, src->format);

    int src_yu_h = src_height/chroma_v_factor;

    if (factor == factor_1) 
    {
        int yu_h = src_height/chroma_v_factor;
        for (int i = 0; i < src_height; ++i) 
            memcpy(dst->data[0] + i * dst->linesize[0], src->data[0] + i * src->linesize[0], src_width);
        for (int i = 0; i < yu_h; ++i)
            memcpy(dst->data[1] + i * dst->linesize[1], src->data[1] + i * src->linesize[1], src_width/chroma_h_factor);
        for (int i = 0; i < yu_h; ++i)
            memcpy(dst->data[2] + i * dst->linesize[2], src->data[2] + i * src->linesize[2], src_width/chroma_h_factor);
    }
    else if (factor == factor_2)
    {
                // perfomance test block
                // last test result: sandy bridge 4.3Ghz. SSE faster than c code at 4-4.5 times for 1920x1080 source
        /*
        QTime t1;
                t1.start();
                volatile int g = 0;
                for (int i = 0; i < 10000; ++i)
                {
            downscalePlate_factor8_sse41_intr(dst->data[0], dst->linesize[0],   src->data[0], src_width, src->linesize[0], src_height, 0);
                        //downscalePlate_factor2_sse(dst->data[0], src_width/2,   src->data[0], src_width, src->linesize[0], src_height);
                        //downscalePlate_factor2_sse(dst->data[1], src_width/chroma_h_factor/2, src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
                        //downscalePlate_factor2_sse(dst->data[2], src_width/chroma_h_factor/2, src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
                        g+=i;
                }
                int e1 = t1.elapsed();

                QTime t2;
                t2.start();
                for (int i = 0; i < 10000; ++i)
                {
            downscalePlate_factor8_sse(dst->data[0], dst->linesize[0],   src->data[0], src_width, src->linesize[0], src_height);
                        //downscalePlate_factor2(dst->data[0], src->data[0], src_width, src->linesize[0], src_height);
                        //downscalePlate_factor2(dst->data[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
                        //downscalePlate_factor2(dst->data[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
                        g+=i;
                }
                int e2 = t2.elapsed();
                */
        if (useSSE2()) {
            downscalePlate_factor2_sse2_intr(dst->data[0], dst->linesize[0],   src->data[0], src_width, src->linesize[0], src_height, 0x00);
            downscalePlate_factor2_sse2_intr(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h, 0x80);
            downscalePlate_factor2_sse2_intr(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h, 0x80);
        } else {
            downscalePlate_factor2(dst->data[0], dst->linesize[0],   src->data[0], src_width, src->linesize[0], src_height);
            downscalePlate_factor2(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
            downscalePlate_factor2(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
        }
    }
    else if(factor == factor_4)
    {
        if (useSSSE3()) {
            downscalePlate_factor4_ssse3_intr(dst->data[0], dst->linesize[0], src->data[0], src_width, src->linesize[0], src->height, 0);
            downscalePlate_factor4_ssse3_intr(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h, 0x80);
            downscalePlate_factor4_ssse3_intr(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h, 0x80);
        } else {
            downscalePlate_factor4(dst->data[0], dst->linesize[0], src->data[0], src_width, src->linesize[0], src->height);
            downscalePlate_factor4(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
            downscalePlate_factor4(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
        }
    }
    else if(factor == factor_8)
    {
        if (useSSE41()) {
            downscalePlate_factor8_sse41_intr(dst->data[0], dst->linesize[0], src->data[0], src_width, src->linesize[0], src->height, 0);
            downscalePlate_factor8_sse41_intr(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h, 0x80);
            downscalePlate_factor8_sse41_intr(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h, 0x80);
        } else {
            downscalePlate_factor8(dst->data[0], dst->linesize[0], src->data[0], src_width, src->linesize[0], src->height);
            downscalePlate_factor8(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
            downscalePlate_factor8(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
        }
    }
    else 
    {
        NX_ASSERT(false);
    }
}

void QnFrameScaler::downscalePlate_factor2(unsigned char* dst, int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height)
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

void QnFrameScaler::downscalePlate_factor4(unsigned char* dst,  int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height)
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


void QnFrameScaler::downscalePlate_factor8(unsigned char* dst,  int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height)
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
