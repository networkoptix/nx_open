// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "yuvconvert.h"

#include <QtCore/private/qsimd_p.h>
#include <nx/media/sse_helper.h>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#if defined(NX_SSE2_SUPPORTED)

    #include <cstdio>
    #include <cstdlib>

    #include <nx/utils/math/math.h>

// Unused: const __m128i  sse_00ffw_intrs = _mm_setr_epi32(0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff);
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
    // This NX_ASSERT does not work in layout background setup.
    // NX_ASSERT( qPower2Ceil((unsigned int)width, 8) == (unsigned int)width );

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
            // calc average for UV
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

            // calculate average RGB values for UV pixels
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

#elif defined(__arm__) && defined(__ARM_NEON)

void yuv444_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    //TODO: ARM.
}

void yuv422_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    //TODO: ARM.
}

void yuv420_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    //TODO: ARM.
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool flip)
{
    //TODO: ARM.
}

/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
void bgra_to_yv12_simd_intr(const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height, bool flip)
{
    //TODO: ARM.
}

//!Converts bgra to yuv420 with alpha plane, total 20 bits per pixel (Y - 8bit, A - 8bit, UV)
/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
void bgra_to_yva12_simd_intr(const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, quint8* a, int yStride, int uvStride, int aStride, int width, int height, bool flip)
{
    //TODO: ARM.
}

#else

void yuv444_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    // TODO: C fallback routine.
}

void yuv422_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    // TODO: C fallback routine.
}

void yuv420_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha)
{
    // TODO: C fallback routine.
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool flip)
{
    // TODO: C fallback routine.
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
    // TODO: C fallback routine.
}

#endif

#if QT_COMPILER_SUPPORTS_HERE(AVX2)

// /*
// Equations:
// R = Y + 1.5748 * (Cr - 128)
// G = Y - 0.187324* (Cb - 128) - 0.468124*(Cr - 128)
// B = Y + 1.8556 * (Cb - 128)
// */

QT_FUNCTION_TARGET(AVX2)
static inline __m128i  pick16Bytes(const uint8_t* base, const Shuffle16& data)
{
    __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + data.offsets[0]));
    const __m128i mask = _mm_load_si128(reinterpret_cast<const __m128i*>(&data.masks[0]));
    value = _mm_shuffle_epi8(value, mask);
    for (int i = 1; i < (int) data.offsets.size(); i++)
    {
        __m128i value2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + data.offsets[i]));
        const __m128i mask2 = _mm_load_si128(reinterpret_cast<const __m128i*>(&data.masks[i]));
        value2 = _mm_shuffle_epi8(value2, mask2);
        value = _mm_or_si128(value2, value);
    }
    return value;
}

static inline __m256i loadPixelsToI16(const uint8_t* base, const Shuffle16& data)
{
    return _mm256_cvtepu8_epi16(pick16Bytes(base, data));  // AVX2: zero-extend to 16-bit
}

// Center chroma: (0..255) -> (-128..127) in 16-bit
QT_FUNCTION_TARGET(AVX2)
static inline __m256i centerChromaU16(__m256i x_u16)
{
    return _mm256_sub_epi16(x_u16, _mm256_set1_epi16(128));
}

// r16/g16/b16 -> 16x f32 in [0,1], with saturation to [0,255]
QT_FUNCTION_TARGET(AVX2)
static inline void store_u8unit_from_i16(float* dst, __m256i v16)
{
    // Extract lanes and pack with unsigned saturation (per 128b lane)
    const __m128i lo16 = _mm256_castsi256_si128(v16);
    const __m128i hi16 = _mm256_extracti128_si256(v16, 1);
    const __m128i u8x16 = _mm_packus_epi16(lo16, hi16);  // 16 x u8 (lower 16 bytes)

    // AVX2 fallback: widen 8+8 bytes separately
    const __m256i lo32 = _mm256_cvtepu8_epi32(u8x16);                 // bytes 0..7
    const __m256i hi32 = _mm256_cvtepu8_epi32(_mm_srli_si128(u8x16, 8)); // 8..15
    const __m256  scale = _mm256_set1_ps(1.0f / 255.0f);
    _mm256_storeu_ps(dst, _mm256_mul_ps(_mm256_cvtepi32_ps(lo32), scale));
    _mm256_storeu_ps(dst + 8, _mm256_mul_ps(_mm256_cvtepi32_ps(hi32), scale));
}

QT_FUNCTION_TARGET(AVX2)
void yuv_to_rgb_planarfp32_avx(
    const YuvToRgbScaleContext* context,
    const uint8_t* yp,
    const uint8_t* up,
    const uint8_t* vp,
    float* rp,
    float* gp,
    float* bp)
{
    // BT.601 full-range, Q14
    const __m256i kCrR = _mm256_set1_epi16(22970); // 1.4020 * 16384
    const __m256i kCbB = _mm256_set1_epi16(29032); // 1.7720 * 16384
    const __m256i kCbG = _mm256_set1_epi16(5638); // 0.344136 * 16384
    const __m256i kCrG = _mm256_set1_epi16(11698); // 0.714136 * 16384

    for (int row = 0; row < context->dstHeight; ++row)
    {
        for (int x = 0; x < context->dstWidth/16; ++x)
        {
            const __m256i y16 = loadPixelsToI16(yp, context->xShufleY[x]);           // 0..255 in i16
            const __m256i cb16 = centerChromaU16(loadPixelsToI16(up, context->xShufleUV[x])); // [-128..127]
            const __m256i cr16 = centerChromaU16(loadPixelsToI16(vp, context->xShufleUV[x]));

            // 1) Scale chroma by 2 to compensate for mulhrs >> 15 with Q14 coeffs
            const __m256i cb2 = _mm256_slli_epi16(cb16, 1);
            const __m256i cr2 = _mm256_slli_epi16(cr16, 1);

            // 2) Products in 16-bit with rounding (mulhrs does (a*b + 0x4000) >> 15, sat)
            const __m256i cr_r = _mm256_mulhrs_epi16(cr2, kCrR); //< ~ 1.402 * Cr
            const __m256i cb_b = _mm256_mulhrs_epi16(cb2, kCbB); //< ~ 1.772 * Cb
            const __m256i cr_g = _mm256_mulhrs_epi16(cr2, kCrG); //< ~ 0.714 * Cr
            const __m256i cb_g = _mm256_mulhrs_epi16(cb2, kCbG); //< ~ 0.344 * Cb

            // combine with Y
            const __m256i r16 = _mm256_add_epi16(y16, cr_r);
            const __m256i b16 = _mm256_add_epi16(y16, cb_b);
            const __m256i g16 = _mm256_sub_epi16(y16, _mm256_add_epi16(cb_g, cr_g));

            // saturate->float[0,1] and store
            store_u8unit_from_i16(rp, r16);
            store_u8unit_from_i16(gp, g16);
            store_u8unit_from_i16(bp, b16);

            rp += 16; gp += 16; bp += 16;
        }
        yp += context->lineStepY[row];
        up += context->lineStepUV[row];
        vp += context->lineStepUV[row];
    }
}

#endif

void YuvToRgbScaleContext::init(
    int srcWidth, int srcHeight,
    int dstWidth, int dstHeight,
    int yLineSize, int uvLineSize,
    int srcFormat)
{
    this->srcWidth = srcWidth;
    this->srcHeight = srcHeight;
    this->dstWidth = dstWidth;
    this->dstHeight = dstHeight;
    this->srcFormat = srcFormat;

    float xStep = (float)srcWidth / dstWidth;
    float yStep = (float)srcHeight / dstHeight;

    float xDownscale = 2.0;
    float yDownscale = 2.0;

    const auto pixelFormat = static_cast<AVPixelFormat>(srcFormat);
    const AVPixFmtDescriptor *desc =
        av_pix_fmt_desc_get(pixelFormat);
    if (desc) {
        xDownscale = static_cast<float>(1 << desc->log2_chroma_w);
        yDownscale = static_cast<float>(1 << desc->log2_chroma_h);
    }

    auto makeShufle = [](int dstWidth, float xStep,  auto& shufleData)
        {
            const float kBias = 0.4999f;
            if(!NX_ASSERT(dstWidth%16 == 0))
            {
                dstWidth = dstWidth - (dstWidth%16);
            }
            shufleData.resize(dstWidth / 16);
            for (int x = 0; x < dstWidth; x += 16)
            {
                for (int k = 0; k < 16;)
                {
                    std::array<uint8_t, 16> maskY;
                    memset(maskY.data(), 128, 16); // zero mask
                    int baseSrcX = int(xStep * (x + k) + kBias);
                    for (; k < 16; ++k)
                    {
                        int srcX = int(xStep * (x + k) + kBias);
                        if (srcX - baseSrcX >= 16)
                            break; // next mm128 register filled.
                        maskY[k] = srcX - baseSrcX;
                    }
                    shufleData[x/16].offsets.push_back(baseSrcX);
                    shufleData[x/16].masks.push_back(maskY);
                }
            }
        };
    makeShufle(dstWidth, xStep, xShufleY);
    makeShufle(dstWidth, xStep/xDownscale, xShufleUV);

    int prevY = 0, prevUV = 0;
    lineStepY.resize(dstHeight);
    lineStepUV.resize(dstHeight);
    for (int i = 0; i < dstHeight; ++i)
    {
        const auto y = std::lround(i* yStep);
        const auto uv = std::lround(i * yStep / yDownscale);
        lineStepY[i] = (y - prevY) * yLineSize;
        lineStepUV[i] = (uv - prevUV) * uvLineSize;
        prevY = y;
        prevUV = uv;
    }
}


void yuv_to_rgb_planarfp32_no_avx(
    const YuvToRgbScaleContext* context,
    const uint8_t* yp,
    const uint8_t* up,
    const uint8_t* vp,
    float* rp,
    float* gp,
    float* bp)
{
    int xDownscale = 2;
    // int yDownscale = 2;

    const auto pixelFormat = static_cast<AVPixelFormat>(context->srcFormat);
    const AVPixFmtDescriptor *desc =
        av_pix_fmt_desc_get(pixelFormat);
    if (desc) {
        xDownscale = static_cast<int>(1 << desc->log2_chroma_w);
        // yDownscale = static_cast<int>(1 << desc->log2_chroma_h);
    }

    // Set up pointers and strides for YUV420p (FFmpeg expects: Y, U, V order)
    const uint8_t* srcData[3] = { yp, up, vp };
    const int srcStride[3] = { context->srcWidth, context->srcWidth / xDownscale, context->srcWidth / xDownscale };

    // Set up pointers and strides for planar RGB32F (FFmpeg pixel format: AV_PIX_FMT_GBRPF32)
    uint8_t *dstData[3] = {reinterpret_cast<uint8_t*>(gp),
                           reinterpret_cast<uint8_t*>(bp),
                           reinterpret_cast<uint8_t*>(rp)};
    const int dstStride[3] = {context->dstWidth * 4, context->dstWidth * 4,
                              context->dstWidth * 4};

    SwsContext *ctx =
        sws_getContext(context->srcWidth, context->srcHeight,
                       (AVPixelFormat)context->srcFormat, context->dstWidth,
                       context->dstHeight, AV_PIX_FMT_GBRPF32,
                       SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    if (!ctx)
      return;

    sws_scale(ctx, srcData, srcStride, 0, context->srcHeight, dstData, dstStride);

    sws_freeContext(ctx);
}

void yuv_to_rgb_planarfp32(
    const YuvToRgbScaleContext* context,
    const uint8_t* yp,
    const uint8_t* up,
    const uint8_t* vp,
    float* rp,
    float* gp,
    float* bp)
{
    static_assert(sizeof(float) == 4);
#if QT_COMPILER_SUPPORTS_HERE(AVX2)
    if (qCpuHasFeature(AVX2))
        return yuv_to_rgb_planarfp32_avx(context, yp, up, vp, rp, gp, bp);
#endif
    yuv_to_rgb_planarfp32_no_avx(context, yp, up, vp, rp, gp, bp);
}
