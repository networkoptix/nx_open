#include <stdio.h>
#include <stdlib.h>
#include <QtGlobal>


#include "utils/common/util.h"
#include <emmintrin.h>

const __m128i  sse_00ffw_intrs = _mm_setr_epi32(0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff);
const __m128i  sse_0010_intrs  = _mm_setr_epi32(0x00100010, 0x00100010, 0x00100010, 0x00100010);
const __m128i  sse_0080_intrs  = _mm_setr_epi32(0x00800080, 0x00800080, 0x00800080, 0x00800080);
const __m128i  sse_0000_intrs  = _mm_setr_epi32(0x00000000, 0x00000000, 0x00000000, 0x00000000);

const __m128i  sse_y_coeff_intrs   = _mm_setr_epi32(0x253f253f, 0x253f253f, 0x253f253f, 0x253f253f);
const __m128i  sse_bu_coeff_intrs  = _mm_setr_epi32(0x40934093, 0x40934093, 0x40934093, 0x40934093);
const __m128i  sse_rv_coeff_intrs  = _mm_setr_epi32(0x33123312, 0x33123312, 0x33123312, 0x33123312);
const __m128i  sse_gu_coeff_intrs  = _mm_setr_epi32(0xf37df37d, 0xf37df37d, 0xf37df37d, 0xf37df37d);
const __m128i  sse_gv_coeff_intrs  = _mm_setr_epi32(0xe5fce5fc, 0xe5fce5fc, 0xe5fce5fc, 0xe5fce5fc);

void yuv444_argb32_sse2_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    Q_ASSERT(roundUp(width, 16) <= y_stride && roundUp(width*4,64) <= dst_stride);
    __m128i sse_alpha_intrs = _mm_setr_epi8(alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha); /* SSE2. */
    int xSteps = roundUp(width, 16) / 16;

    for (int y = height; y > 0; --y)
    {
        const __m128i *srcY = (const __m128i*)  py;
        const __m128i *srcU =  (const __m128i*) pu;
        const __m128i *srcV =  (const __m128i*) pv;
        __m128i* dstIntr = (__m128i*) dst;
        for (int x = xSteps; x > 0; --x)
        {
            __m128i y00 = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcY), sse_0000_intrs),sse_0010_intrs); /* SSE2. */
            __m128i y01 = _mm_sub_epi16(_mm_unpackhi_epi8(_mm_loadu_si128(srcY++), sse_0000_intrs),sse_0010_intrs); /* SSE2. */

            y00 = _mm_mulhi_epi16(_mm_slli_epi16(y00,3), sse_y_coeff_intrs); /* SSE2. */
            y01 = _mm_mulhi_epi16(_mm_slli_epi16(y01,3), sse_y_coeff_intrs); /* SSE2. */

            __m128i u00 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcU), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */
            __m128i u01 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpackhi_epi8(_mm_loadu_si128(srcU++), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */

            __m128i bu0 = _mm_mulhi_epi16(u00, sse_bu_coeff_intrs); /* SSE2. */
            __m128i bu1 = _mm_mulhi_epi16(u01, sse_bu_coeff_intrs); /* SSE2. */

            __m128i v00 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcV), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */
            __m128i v01 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpackhi_epi8(_mm_loadu_si128(srcV++), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */

            __m128i rv0 = _mm_mulhi_epi16(v00, sse_rv_coeff_intrs); /* SSE2. */
            __m128i rv1 = _mm_mulhi_epi16(v01, sse_rv_coeff_intrs); /* SSE2. */

            u00 = _mm_mulhi_epi16(u00, sse_gu_coeff_intrs); /* SSE2. */
            u01 = _mm_mulhi_epi16(u01, sse_gu_coeff_intrs); /* SSE2. */
            v00 = _mm_mulhi_epi16(v00, sse_gv_coeff_intrs); /* SSE2. */
            v01 = _mm_mulhi_epi16(v01, sse_gv_coeff_intrs); /* SSE2. */

            __m128i r00 = _mm_add_epi16(y00, rv0); /* SSE2. */
            __m128i r01 = _mm_add_epi16(y01, rv1); /* SSE2. */
            __m128i g00 = _mm_add_epi16(_mm_add_epi16(y00, v00), u00); /* SSE2. */
            __m128i g01 = _mm_add_epi16(_mm_add_epi16(y01, v01), u01); /* SSE2. */
            __m128i b00 = _mm_add_epi16(y00, bu0); /* SSE2. */
            __m128i b01 = _mm_add_epi16(y01, bu1); /* SSE2. */

            r00 = _mm_packus_epi16(r00, r01); /* SSE2. */
            g00 = _mm_packus_epi16(g00, g01); /* SSE2. */
            b00 = _mm_packus_epi16(b00, b01); /* SSE2. */

            __m128i bg0 = _mm_unpacklo_epi8(b00,g00); /* SSE2. */
            __m128i ra0 = _mm_unpacklo_epi8(r00, sse_alpha_intrs); /* SSE2. */
            __m128i bg1 = _mm_unpackhi_epi8(b00,g00); /* SSE2. */
            __m128i ra1 = _mm_unpackhi_epi8(r00, sse_alpha_intrs); /* SSE2. */

            *dstIntr++ =_mm_unpacklo_epi16(bg0, ra0); /* SSE2. */
            *dstIntr++ =_mm_unpackhi_epi16(bg0, ra0); /* SSE2. */
            *dstIntr++ =_mm_unpacklo_epi16(bg1, ra1); /* SSE2. */
            *dstIntr++ =_mm_unpackhi_epi16(bg1, ra1); /* SSE2. */
        }
        py += y_stride;
        pu += uv_stride;
        pv += uv_stride;
        dst += dst_stride;
    }
}

void yuv422_argb32_sse2_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    Q_ASSERT(roundUp(width, 16) <= y_stride && roundUp(width*4,64) <= dst_stride);
    __m128i sse_alpha_intrs = _mm_setr_epi8(alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha); /* SSE2. */
    int xSteps = roundUp(width, 16) / 16;

    for (int y = height; y > 0; --y)
    {
        const __m128i *srcY = (const __m128i*)  py;
        __m128i* dstIntr = (__m128i*) dst;

        const quint8* curPu = pu;
        const quint8* curPv = pv;
        for (int x = xSteps; x > 0; --x)
        {
            const __m128i *srcU =  (const __m128i*) curPu;
            const __m128i *srcV =  (const __m128i*) curPv;

            __m128i y00 = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcY), sse_0000_intrs),sse_0010_intrs); /* SSE2. */
            __m128i y01 = _mm_sub_epi16(_mm_unpackhi_epi8(_mm_loadu_si128(srcY++), sse_0000_intrs),sse_0010_intrs); /* SSE2. */

            y00 = _mm_mulhi_epi16(_mm_slli_epi16(y00,3), sse_y_coeff_intrs); /* SSE2. */
            y01 = _mm_mulhi_epi16(_mm_slli_epi16(y01,3), sse_y_coeff_intrs); /* SSE2. */

            __m128i u00 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcU), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */
            __m128i bu0 = _mm_mulhi_epi16(u00, sse_bu_coeff_intrs); /* SSE2. */
            __m128i v00 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcV), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */
            __m128i rv0 = _mm_mulhi_epi16(v00, sse_rv_coeff_intrs); /* SSE2. */
            u00 = _mm_mulhi_epi16(u00, sse_gu_coeff_intrs); /* SSE2. */
            v00 = _mm_mulhi_epi16(v00, sse_gv_coeff_intrs); /* SSE2. */

            __m128i  rv1 = _mm_unpackhi_epi16(rv0, rv0); /* SSE2. */
            rv0 = _mm_unpacklo_epi16(rv0, rv0); /* SSE2. */
            __m128i  bu1 = _mm_unpackhi_epi16(bu0, bu0); /* SSE2. */
            bu0 = _mm_unpacklo_epi16(bu0, bu0); /* SSE2. */
            __m128i  u01 = _mm_unpackhi_epi16(u00, u00); /* SSE2. */
            u00 = _mm_unpacklo_epi16(u00, u00); /* SSE2. */
            __m128i  v01 = _mm_unpackhi_epi16(v00, v00); /* SSE2. */
            v00 = _mm_unpacklo_epi16(v00, v00); /* SSE2. */

            __m128i r00 = _mm_add_epi16(y00, rv0); /* SSE2. */
            __m128i r01 = _mm_add_epi16(y01, rv1); /* SSE2. */
            __m128i g00 = _mm_add_epi16(_mm_add_epi16(u00, v00), y00); /* SSE2. */
            __m128i g01 = _mm_add_epi16(_mm_add_epi16(u01, v01), y01); /* SSE2. */
            __m128i b00 = _mm_add_epi16(y00, bu0); /* SSE2. */
            __m128i b01 = _mm_add_epi16(y01, bu1); /* SSE2. */

            r00 = _mm_packus_epi16(r00, r01); /* SSE2. */
            g00 = _mm_packus_epi16(g00, g01); /* SSE2. */
            b00 = _mm_packus_epi16(b00, b01); /* SSE2. */

            __m128i bg0 = _mm_unpacklo_epi8(b00,g00); /* SSE2. */
            __m128i ra0 = _mm_unpacklo_epi8(r00, sse_alpha_intrs); /* SSE2. */
            __m128i bg1 = _mm_unpackhi_epi8(b00,g00); /* SSE2. */
            __m128i ra1 = _mm_unpackhi_epi8(r00, sse_alpha_intrs); /* SSE2. */

            *dstIntr++ =_mm_unpacklo_epi16(bg0, ra0); /* SSE2. */
            *dstIntr++ =_mm_unpackhi_epi16(bg0, ra0); /* SSE2. */
            *dstIntr++ =_mm_unpacklo_epi16(bg1, ra1); /* SSE2. */
            *dstIntr++ =_mm_unpackhi_epi16(bg1, ra1); /* SSE2. */
            curPu += 8;
            curPv += 8;
        }
        py += y_stride;
        pu += uv_stride;
        pv += uv_stride;
        dst += dst_stride;
    }
}

void yuv420_argb32_sse2_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    Q_ASSERT(roundUp(width, 16) <= y_stride && roundUp(width*4,64) <= dst_stride);
    __m128i sse_alpha_intrs = _mm_setr_epi8(alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha); /* SSE2. */
    int xSteps = roundUp(width, 16) / 16;
    __m128i u00, u01, bu0, bu1, v00, v01, rv0, rv1;

    for (unsigned int y = 0; y < height/2; ++y)
    {
        const __m128i *srcY = (const __m128i*)  py;
        __m128i* dstIntr = (__m128i*) dst;

        const quint8* curPu = pu;
        const quint8* curPv = pv;
        for (int x = xSteps; x > 0; --x)
        {
            const __m128i *srcU =  (const __m128i*) curPu;
            const __m128i *srcV =  (const __m128i*) curPv;

            //if ((y & 1) == 0)
            {
                u00 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcU), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */
                bu0 = _mm_mulhi_epi16(u00, sse_bu_coeff_intrs); /* SSE2. */
                v00 = _mm_slli_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcV), sse_0000_intrs),sse_0080_intrs),3); /* SSE2. */
                rv0 = _mm_mulhi_epi16(v00, sse_rv_coeff_intrs); /* SSE2. */
                u00 = _mm_mulhi_epi16(u00, sse_gu_coeff_intrs); /* SSE2. */
                v00 = _mm_mulhi_epi16(v00, sse_gv_coeff_intrs); /* SSE2. */
                rv1 = _mm_unpackhi_epi16(rv0, rv0); /* SSE2. */
                rv0 = _mm_unpacklo_epi16(rv0, rv0); /* SSE2. */
                bu1 = _mm_unpackhi_epi16(bu0, bu0); /* SSE2. */
                bu0 = _mm_unpacklo_epi16(bu0, bu0); /* SSE2. */
                u01 = _mm_unpackhi_epi16(u00, u00); /* SSE2. */
                u00 = _mm_unpacklo_epi16(u00, u00); /* SSE2. */
                v01 = _mm_unpackhi_epi16(v00, v00); /* SSE2. */
                v00 = _mm_unpacklo_epi16(v00, v00); /* SSE2. */
            }
            // step 1
            __m128i y00 = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128(srcY), sse_0000_intrs),sse_0010_intrs); /* SSE2. */
            __m128i y01 = _mm_sub_epi16(_mm_unpackhi_epi8(_mm_loadu_si128(srcY), sse_0000_intrs),sse_0010_intrs); /* SSE2. */

            y00 = _mm_mulhi_epi16(_mm_slli_epi16(y00,3), sse_y_coeff_intrs); /* SSE2. */
            y01 = _mm_mulhi_epi16(_mm_slli_epi16(y01,3), sse_y_coeff_intrs); /* SSE2. */
            __m128i r00 = _mm_add_epi16(y00, rv0); /* SSE2. */
            __m128i r01 = _mm_add_epi16(y01, rv1); /* SSE2. */
            __m128i g00 = _mm_add_epi16(_mm_add_epi16(u00, v00), y00); /* SSE2. */
            __m128i g01 = _mm_add_epi16(_mm_add_epi16(u01, v01), y01); /* SSE2. */
            __m128i b00 = _mm_add_epi16(y00, bu0); /* SSE2. */
            __m128i b01 = _mm_add_epi16(y01, bu1); /* SSE2. */

            r00 = _mm_packus_epi16(r00, r01); /* SSE2. */
            g00 = _mm_packus_epi16(g00, g01); /* SSE2. */
            b00 = _mm_packus_epi16(b00, b01); /* SSE2. */

            __m128i bg0 = _mm_unpacklo_epi8(b00,g00); /* SSE2. */
            __m128i ra0 = _mm_unpacklo_epi8(r00, sse_alpha_intrs); /* SSE2. */
            __m128i bg1 = _mm_unpackhi_epi8(b00,g00); /* SSE2. */
            __m128i ra1 = _mm_unpackhi_epi8(r00, sse_alpha_intrs); /* SSE2. */

            dstIntr[0] =_mm_unpacklo_epi16(bg0, ra0); /* SSE2. */
            dstIntr[1] =_mm_unpackhi_epi16(bg0, ra0); /* SSE2. */
            dstIntr[2] =_mm_unpacklo_epi16(bg1, ra1); /* SSE2. */
            dstIntr[3] =_mm_unpackhi_epi16(bg1, ra1); /* SSE2. */

            // step 2
            const __m128i* y2 = (const __m128i*) (((const char*) srcY) + y_stride);
            __m128i* dst2 = (__m128i*) (((char*) dstIntr) + dst_stride);
            y00 = _mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadu_si128( y2), sse_0000_intrs),sse_0010_intrs); /* SSE2. */
            y01 = _mm_sub_epi16(_mm_unpackhi_epi8(_mm_loadu_si128(y2), sse_0000_intrs),sse_0010_intrs); /* SSE2. */

            y00 = _mm_mulhi_epi16(_mm_slli_epi16(y00,3), sse_y_coeff_intrs); /* SSE2. */
            y01 = _mm_mulhi_epi16(_mm_slli_epi16(y01,3), sse_y_coeff_intrs); /* SSE2. */
            r00 = _mm_add_epi16(y00, rv0); /* SSE2. */
            r01 = _mm_add_epi16(y01, rv1); /* SSE2. */
            g00 = _mm_add_epi16(_mm_add_epi16(u00, v00), y00); /* SSE2. */
            g01 = _mm_add_epi16(_mm_add_epi16(u01, v01), y01); /* SSE2. */
            b00 = _mm_add_epi16(y00, bu0); /* SSE2. */
            b01 = _mm_add_epi16(y01, bu1); /* SSE2. */

            r00 = _mm_packus_epi16(r00, r01); /* SSE2. */
            g00 = _mm_packus_epi16(g00, g01); /* SSE2. */
            b00 = _mm_packus_epi16(b00, b01); /* SSE2. */

            bg0 = _mm_unpacklo_epi8(b00,g00); /* SSE2. */
            ra0 = _mm_unpacklo_epi8(r00, sse_alpha_intrs); /* SSE2. */
            bg1 = _mm_unpackhi_epi8(b00,g00); /* SSE2. */
            ra1 = _mm_unpackhi_epi8(r00, sse_alpha_intrs); /* SSE2. */

            dst2[0] =_mm_unpacklo_epi16(bg0, ra0); /* SSE2. */
            dst2[1] =_mm_unpackhi_epi16(bg0, ra0); /* SSE2. */
            dst2[2] =_mm_unpacklo_epi16(bg1, ra1); /* SSE2. */
            dst2[3] =_mm_unpackhi_epi16(bg1, ra1); /* SSE2. */


            curPu += 8;
            curPv += 8;
            dstIntr += 4;
            srcY++;
        }
        py += y_stride*2;
        pu += uv_stride;
        pv += uv_stride;
        dst += dst_stride*2;
    }
}
