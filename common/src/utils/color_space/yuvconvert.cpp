
#if defined(__i386) || defined(__amd64) || defined(_WIN32)

#include <emmintrin.h>

#include <cstdio>
#include <cstdlib>

#include <QtCore/QtGlobal>

#include "utils/math/math.h"


const __m128i  sse_00ffw_intrs = _mm_setr_epi32(0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff);
const __m128i  sse_0010_intrs  = _mm_setr_epi32(0x00100010, 0x00100010, 0x00100010, 0x00100010);
const __m128i  sse_0080_intrs  = _mm_setr_epi32(0x00800080, 0x00800080, 0x00800080, 0x00800080);
const __m128i  sse_0000_intrs  = _mm_setr_epi32(0x00000000, 0x00000000, 0x00000000, 0x00000000);

const __m128i  sse_y_coeff_intrs   = _mm_setr_epi32(0x253f253f, 0x253f253f, 0x253f253f, 0x253f253f);
const __m128i  sse_bu_coeff_intrs  = _mm_setr_epi32(0x40934093, 0x40934093, 0x40934093, 0x40934093);
const __m128i  sse_rv_coeff_intrs  = _mm_setr_epi32(0x33123312, 0x33123312, 0x33123312, 0x33123312);
const __m128i  sse_gu_coeff_intrs  = _mm_setr_epi32(0xf37df37d, 0xf37df37d, 0xf37df37d, 0xf37df37d);
const __m128i  sse_gv_coeff_intrs  = _mm_setr_epi32(0xe5fce5fc, 0xe5fce5fc, 0xe5fce5fc, 0xe5fce5fc);

void yuv444_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    NX_ASSERT(qPower2Ceil(width, 16) <= y_stride && qPower2Ceil(width*4,64) <= dst_stride);
    __m128i sse_alpha_intrs = _mm_setr_epi8(alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha); /* SSE2. */
    int xSteps = qPower2Ceil(width, 16) / 16;

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

void yuv422_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    NX_ASSERT(qPower2Ceil(width, 16) <= y_stride && qPower2Ceil(width*4,64) <= dst_stride);
    __m128i sse_alpha_intrs = _mm_setr_epi8(alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha); /* SSE2. */
    int xSteps = qPower2Ceil(width, 16) / 16;

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

void yuv420_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    NX_ASSERT(qPower2Ceil(width, 16) <= y_stride);
    NX_ASSERT(qPower2Ceil(width*4,64) <= dst_stride);

    __m128i sse_alpha_intrs = _mm_setr_epi8(alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha); /* SSE2. */
    int xSteps = qPower2Ceil(width, 16) / 16;
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

void bgra_to_yv12_simd_intr(const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height, bool flip)
{
    // this NX_ASSERT does not work in layout background setup --gdm
    //NX_ASSERT( qPower2Ceil((unsigned int)width, 8) == (unsigned int)width );

    static const __m128i sse_2000         = _mm_setr_epi16( 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020 ); /* SSE2. */
    static const __m128i sse_00a0         = _mm_setr_epi16( 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210 ); /* SSE2. */
    static const __m128i sse_mask_color   = _mm_setr_epi32( 0x00003fc0, 0x00003fc0, 0x00003fc0, 0x00003fc0); /* SSE2. */
    static const __m128i sse_01           = _mm_setr_epi16(0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001); /* SSE2. */

    static const int K = 32768;
    static const __m128i y_r_coeff =  _mm_setr_epi16( 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K ); /* SSE2. */
    static const __m128i y_g_coeff =  _mm_setr_epi16( 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K ); /* SSE2. */
    static const __m128i y_b_coeff =  _mm_setr_epi16( 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K ); /* SSE2. */

    static const __m128i uv_r_coeff = _mm_setr_epi16( 0.439*K,  0.439*K,  0.439*K,  0.439*K,    -0.148*K, -0.148*K, -0.148*K, -0.148*K ); /* SSE2. */
    static const __m128i uv_g_coeff = _mm_setr_epi16(-0.368*K, -0.368*K, -0.368*K, -0.368*K,    -0.291*K, -0.291*K, -0.291*K, -0.291*K ); /* SSE2. */
    static const __m128i uv_b_coeff = _mm_setr_epi16(-0.071*K, -0.071*K, -0.071*K, -0.071*K,     0.439*K,  0.439*K,  0.439*K,  0.439*K ); /* SSE2. */

    int xSteps = qPower2Ceil((unsigned int)width, 8)/8;
    NX_ASSERT( xSteps*4 <= xStride );
    if (flip)
    {
        y += yStride*(height-1);
        u += uvStride*(height/2-1);
        v += uvStride*(height/2-1);
        yStride = -yStride;
        uvStride = -uvStride;
    }

    for (int yLine = 0; yLine < height/2; ++yLine)
    {
        const __m128i *srcLine1 = (const __m128i*) rgba;
        const __m128i *srcLine2 = (const __m128i*) (rgba + xStride);
        quint8* dstY = y;
        quint32* dstU = (quint32*) u;
        quint32* dstV = (quint32*) v;

        for (int x = xSteps; x > 0; --x)
        {
            // prepare first line
            __m128i rgbaB0 = _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine1),6), sse_mask_color); /* SSE2. */
            rgbaB0 = _mm_packs_epi32(rgbaB0, _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine1+1),6), sse_mask_color)); /* SSE2. */
            __m128i rgbaG0 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1),2), sse_mask_color); /* SSE2. */
            rgbaG0 = _mm_packs_epi32(rgbaG0, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1+1),2), sse_mask_color)); /* SSE2. */
            __m128i rgbaR0 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1),10), sse_mask_color); /* SSE2. */
            rgbaR0 = _mm_packs_epi32(rgbaR0, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine1+1),10), sse_mask_color)); /* SSE2. */
            // prepare second line
            __m128i rgbaB1 = _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine2),6), sse_mask_color); /* SSE2. */
            rgbaB1 = _mm_packs_epi32(rgbaB1, _mm_and_si128(_mm_slli_epi32(_mm_loadu_si128(srcLine2+1),6), sse_mask_color)); /* SSE2. */
            __m128i rgbaG1 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2),2), sse_mask_color); /* SSE2. */
            rgbaG1 = _mm_packs_epi32(rgbaG1, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2+1),2), sse_mask_color)); /* SSE2. */
            __m128i rgbaR1 = _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2),10), sse_mask_color); /* SSE2. */
            rgbaR1 = _mm_packs_epi32(rgbaR1, _mm_and_si128(_mm_srli_epi32(_mm_loadu_si128(srcLine2+1),10), sse_mask_color)); /* SSE2. */
            // calc Y vector
            __m128i yData = _mm_add_epi16(_mm_mulhi_epi16(rgbaB0, y_b_coeff), _mm_add_epi16(_mm_mulhi_epi16(rgbaG0, y_g_coeff), _mm_mulhi_epi16(rgbaR0, y_r_coeff))); /* SSE2. */
            yData = _mm_srli_epi16(_mm_add_epi16(yData, sse_00a0), 5); /* SSE2. */
            _mm_storel_epi64((__m128i*) dstY, _mm_packus_epi16(yData, yData)); /* SSE2. */
            yData = _mm_add_epi16(_mm_mulhi_epi16(rgbaB1, y_b_coeff), _mm_add_epi16(_mm_mulhi_epi16(rgbaG1, y_g_coeff), _mm_mulhi_epi16(rgbaR1, y_r_coeff))); /* SSE2. */
            yData = _mm_srli_epi16(_mm_add_epi16(yData, sse_00a0), 5); /* SSE2. */
            _mm_storel_epi64((__m128i*) (dstY+yStride), _mm_packus_epi16(yData, yData));
            // calc avarage for UV
            __m128i bAvg = _mm_madd_epi16(_mm_avg_epu16(rgbaB0, rgbaB1), sse_01); /* SSE2. */ // use madd instead of "horizontal add" to prevent SSSE3 requirement
            __m128i gAvg = _mm_madd_epi16(_mm_avg_epu16(rgbaG0, rgbaG1), sse_01); /* SSE2. */
            __m128i rAvg = _mm_madd_epi16(_mm_avg_epu16(rgbaR0, rgbaR1), sse_01); /* SSE2. */
            // calc UV vector
            __m128i uv = sse_2000;
            uv = _mm_add_epi16(uv, _mm_mulhi_epi16(_mm_packs_epi32(rAvg, rAvg), uv_r_coeff)); /* SSE2. */
            uv = _mm_add_epi16(uv, _mm_mulhi_epi16(_mm_packs_epi32(gAvg, gAvg), uv_g_coeff)); /* SSE2. */
            uv = _mm_add_epi16(uv, _mm_mulhi_epi16(_mm_packs_epi32(bAvg, bAvg), uv_b_coeff)); /* SSE2. */
            uv = _mm_srli_epi16(uv, 6); /* SSE2. */
            uv = _mm_packus_epi16(uv, uv); /* SSE2. */
            *dstV++ = _mm_cvtsi128_si32(uv); /* SSE2. */
            *dstU++ = _mm_cvtsi128_si32(_mm_srli_epi64(uv, 32)); /* SSE2. */

            dstY += 8;
            srcLine1 += 2;
            srcLine2 += 2;
        }
        u += uvStride;
        v += uvStride;
        y += yStride*2;
        rgba += xStride*2;
    }
}

void bgra_to_yva12_simd_intr(
    const quint8* rgba, int xStride,
    quint8* y, quint8* u, quint8* v, quint8* a,
    int yStride, int uvStride, int aStride,
    int width, int height,
    bool flip )
{
    bgra_to_yv12_simd_intr( rgba, xStride, y, u, v, yStride, uvStride, width, height, flip );

    // copying alpha plane to a
    // TODO: optimize with sse
    for( int yLine = 0; yLine < height; ++yLine )
    {
        for( int x = 0; x < width; ++x )
            *(a+x) = *((rgba + x*4)+3);
        rgba += xStride;
        a += aStride;
    }
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool /*flip*/)
{
    int xStride = width*4;
    quint8* yptr2 = yptr + width;
    quint8* rgba2 = rgba + xStride;
    for (int y = height/2; y > 0; --y)
    {
        for (int x = width/2; x > 0; --x)
        {
            // process 4 Y pixels
            quint8 y1 = rgba[0]*0.098f + rgba[1]*0.504f + rgba[2]*0.257f + 16.5f;
            *yptr++ = y1;
            quint8 y2 = rgba[4]*0.098f + rgba[5]*0.504f + rgba[6]*0.257f + 16.5f;
            *yptr++ = y2;

            quint8 y3 = rgba2[0]*0.098 + rgba2[1]*0.504 + rgba2[2]*0.257 + 16.5f;
            *yptr2++ = y3;
            quint8 y4 = rgba2[4]*0.098 + rgba2[5]*0.504 + rgba2[6]*0.257 + 16.5f;
            *yptr2++ = y4;

            // calculate avarage RGB values for UV pixels
            quint8 avgB = (rgba[0]+rgba[4]+rgba2[0]+rgba2[4]+2) >> 2;
            quint8 avgG = (rgba[1]+rgba[5]+rgba2[1]+rgba2[5]+2) >> 2;
            quint8 avgR = (rgba[2]+rgba[6]+rgba2[2]+rgba2[6]+2) >> 2;

            // write UV pixels
            *vptr++ = 0.439f*avgR  - 0.368f*avgG - 0.071f*avgB + 128.5f;
            *uptr++ = -0.148f*avgR - 0.291f*avgG + 0.439f*avgB + 128.5f;

            rgba += 8;
            rgba2 += 8;
        }
        rgba   += xStride;
        rgba2  += xStride;
        yptr  += width;
        yptr2 += width;
    }
}

#elif __arm__ && __ARM_NEON__

void yuv444_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    //TODO/ARM
}

void yuv422_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    //TODO/ARM
}

void yuv420_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    //TODO/ARM
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool flip)
{
    //TODO/ARM
}

/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
void bgra_to_yv12_simd_intr(const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height, bool flip)
{
    //TODO/ARM
}

//!Converts bgra to yuv420 with alpha plane, total 20 bits per pixel (Y - 8bit, A - 8bit, UV)
/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
void bgra_to_yva12_simd_intr(const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, quint8* a, int yStride, int uvStride, int aStride, int width, int height, bool flip)
{
    //TODO/ARM
}

#else
void yuv444_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    // TODO: C fallback routine
}

void yuv422_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    // TODO: C fallback routine
}

void yuv420_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    // TODO: C fallback routine
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool flip)
{
    // TODO: C fallback routine
}

/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
void bgra_to_yv12_simd_intr(const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height, bool flip)
{
    // TODO: C fallback routine
}

//!Converts bgra to yuv420 with alpha plane, total 20 bits per pixel (Y - 8bit, A - 8bit, UV)
/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
void bgra_to_yva12_simd_intr(const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, quint8* a, int yStride, int uvStride, int aStride, int width, int height, bool flip)
{
    // TODO: C fallback routine
}
#endif
