#include "frame_info.h"
#include "../common/util.h"
#include <emmintrin.h>
#include "sse_helper.h"

#include <string.h>
#include <stdio.h>


extern "C" {
#ifdef WIN32
#define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include "libavutil/pixdesc.h"
#ifdef WIN32
#undef AVPixFmtDescriptor
#endif

};


const __m128i  sse_00ffw_intrs = _mm_setr_epi32(0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff);
const __m128i  sse_000000ffw_intrs = _mm_setr_epi32(0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff);

void downscalePlate_factor2_sse2_intr(unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
                                const unsigned int width, const unsigned int src_stride, unsigned int height, int fillter)
{
    Q_ASSERT(roundUp(width, 16) <= src_stride && roundUp(width/2,16) <= dst_stride);

    const __m128i color_const_intrs = _mm_setr_epi16(fillter, fillter, fillter, fillter, fillter, fillter, fillter, fillter); /* SSE2. */
    int xSteps = roundUp(width, 16) / 16;
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

void downscalePlate_factor4_ssse3_intr(unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
                                     const unsigned int width, const unsigned int src_stride, unsigned int height, int filler)
{
    Q_ASSERT(roundUp(width, 16) <= src_stride && roundUp(width/4,8) <= dst_stride);

    const __m128i color_const_intrs = _mm_setr_epi16(filler, filler, filler, filler, filler, filler, filler, filler); /* SSE2. */
    int xSteps = roundUp(width, 16) / 16;
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

            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16(s00, s02), _mm_hadd_epi16(s10, s12)),1); /* SSSE3. */

             _mm_storel_epi64((__m128i*) dstCur, _mm_packus_epi16(rez1, rez1)); /* SSE2. */
            src1 += 2;
            src2 += 2;
            dstCur += 8;
        }
        if (xSteps & 1)
        {
            __m128i s00 = _mm_and_si128(_mm_loadu_si128(src1), sse_00ffw_intrs); /* SSE2. */
            __m128i s10 = _mm_and_si128(_mm_loadu_si128(src2), sse_00ffw_intrs); /* SSE2. */

            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16(s00, color_const_intrs), _mm_hadd_epi16(s10, color_const_intrs)),1); /* SSSE3. */
            _mm_storel_epi64((__m128i*) dstCur, _mm_packus_epi16(rez1, rez1)); /* SSE2. */
        }
        dst += dst_stride;
        src_line1 += src_stride*4;
        src_line2 += src_stride*4;
    }
}

void downscalePlate_factor8_sse41_intr(unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
                                     const unsigned int width, const unsigned int src_stride, unsigned int height, int filler)
{
    Q_ASSERT(roundUp(width, 16) <= src_stride && roundUp(width/8,4) <= dst_stride);

    const __m128i color_const_intrs = _mm_setr_epi16(filler, filler, filler, filler, filler, filler, filler, filler); /* SSE2. */
    int xSteps = roundUp(width, 16) / 16;
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

            s00 = _mm_packus_epi32(s00, s02); /* SSE4. */
            s10 = _mm_packus_epi32(s10, s12); /* SSE4. */
            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16(s00, s00), _mm_hadd_epi16(s10, s10)),1); /* SSSE3. */

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
            s00 = _mm_packus_epi32(s00, color_const_intrs); /* SSE2. */
            s10 = _mm_packus_epi32(s10, color_const_intrs); /* SSE2. */

            __m128i rez1 = _mm_srli_epi16(_mm_avg_epu16(_mm_hadd_epi16(s00, color_const_intrs), _mm_hadd_epi16(s10, color_const_intrs)),1); /* SSSE3. */
            __m128i tmp = _mm_packus_epi16(rez1, color_const_intrs); /* SSE2. */
            _mm_store_ss((float*) dstCur, *(__m128*)(&tmp)); /* SSE. */
        }
        dst += dst_stride;
        src_line1 += src_stride*8;
        src_line2 += src_stride*8;
    }
}

CLVideoDecoderOutput::CLVideoDecoderOutput()
{
    memset(this, 0, sizeof(*this));
}

CLVideoDecoderOutput::~CLVideoDecoderOutput()
{
    clean();
}

void CLVideoDecoderOutput::setUseExternalData(bool value)
{
    if (value != m_useExternalData)
        clean();
    m_useExternalData = value;
}

void CLVideoDecoderOutput::clean()
{
    if (!m_useExternalData && data[0])
        av_free(data[0]);
    data[0] = data[1] = data[2] = 0;
    linesize[0] = linesize[1] = linesize[2] = 0;
    width = height = 0;
}

void CLVideoDecoderOutput::copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst)
{
    if (src->width != dst->width || src->height != dst->height || src->format != dst->format)
    {
        // need to realocate dst memory 
        dst->setUseExternalData(false);
        int numBytes = avpicture_get_size((PixelFormat) src->format, src->width, src->height);
        avpicture_fill((AVPicture*) dst, (quint8*) av_malloc(numBytes), (PixelFormat) src->format, src->width, src->height);

        /*
        dst->linesize[0] = src->width;
        dst->linesize[1] = src->width/2;
        dst->linesize[2] = src->width/2;

        dst->width = src->width;

        dst->height = src->height;
        dst->format = src->format;

        int yu_h = dst->format == PIX_FMT_YUV420P ? dst->height/2 : dst->height;

        dst-data[0] = (unsigned char*) av_malloc(dst->getCapacity());
        dst->data[1] = dst-data[0] + dst->linesize[0]*dst->height;
        dst->data[2] = dst-data[1] + dst->linesize[1]*yu_h;
        */
    }

    int yu_h = dst->format == PIX_FMT_YUV420P ? dst->height/2 : dst->height;

    copyPlane(dst->data[0], src->data[0], dst->linesize[0], dst->linesize[0], src->linesize[0], src->height);
    copyPlane(dst->data[1], src->data[1], dst->linesize[1], dst->linesize[1], src->linesize[1], yu_h);
    copyPlane(dst->data[2], src->data[2], dst->linesize[2], dst->linesize[2], src->linesize[2], yu_h);

}

/*
int CLVideoDecoderOutput::getCapacity()
{
    int yu_h = format == PIX_FMT_YUV420P ? height/2 : height;
    return linesize[0]*height + (linesize[1] + linesize[2])*yu_h;
}
*/

void CLVideoDecoderOutput::copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride,  int src_stride, int height)
{

    for (int i = 0; i < height; ++i)
    {
        memcpy(dst, src, width);
        dst+=dst_stride;
        src+=src_stride;
    }
}

void CLVideoDecoderOutput::fillRightEdge()
{
    if (format == -1)
        return;
    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[format];
    quint8 filler = 0;
    int w = width;
    int h = height;
    for (int i = 0; i < descr->nb_components && data[i]; ++i)
    {
        int bpp = descr->comp[i].step_minus1 + 1;
        int fillLen = linesize[i] - w*bpp;
        if (fillLen) {
            quint8* dst = data[i] + w*bpp;
            for (int y = 0; y < h; ++y) {
                memset(dst, filler, fillLen);
                dst += linesize[i];
            }
        }
        if (i == 0) {
            filler = 0x80;
            w >>= descr->log2_chroma_w;
            h >>= descr->log2_chroma_h;
        }
    }
}

void CLVideoDecoderOutput::reallocate(int newWidth, int newHeight, int newFormat)
{
    clean();
    setUseExternalData(false);
    width = newWidth;
    height = newHeight;
    format = newFormat;

    int rc = 32 >> (newFormat == PIX_FMT_RGBA || newFormat == PIX_FMT_ABGR || newFormat == PIX_FMT_BGRA ? 2 : 0);
    int roundWidth = roundUp((unsigned) width, rc);
    int numBytes = avpicture_get_size((PixelFormat) format, roundWidth, height);
    if (numBytes > 0) {
        avpicture_fill((AVPicture*) this, (quint8*) av_malloc(numBytes), (PixelFormat) format, roundWidth, height);
        fillRightEdge();
    }
}

void CLVideoDecoderOutput::reallocate(int newWidth, int newHeight, int newFormat, int lineSizeHint)
{
    clean();
    setUseExternalData(false);
    width = newWidth;
    height = newHeight;
    format = newFormat;

    int numBytes = avpicture_get_size((PixelFormat) format, lineSizeHint, height);
    if (numBytes > 0) {
        avpicture_fill((AVPicture*) this, (quint8*) av_malloc(numBytes), (PixelFormat) format, lineSizeHint, height);
        fillRightEdge();
    }
}


void CLVideoDecoderOutput::downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, downscale_factor factor)
{
    int src_width = src->width;
    int src_height = src->height;

    const int chroma_h_factor = (src->format == PIX_FMT_YUV420P || src->format == PIX_FMT_YUV422P) ? 2 : 1;
    const int chroma_v_factor = (src->format == PIX_FMT_YUV420P) ? 2 : 1;

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
        cl_log.log("scale factor 2. sse intr time:", e1, cl_logALWAYS);
		cl_log.log("scale factor 2. sse time:", e2, cl_logALWAYS);
		cl_log.log("-------------------------",  g, cl_logALWAYS);
		*/
        if (useSSE2()) {
            downscalePlate_factor2_sse2_intr(dst->data[0], dst->linesize[0],   src->data[0], src_width, src->linesize[0], src_height, 0x00);
		    downscalePlate_factor2_sse2_intr(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h, 0x80);
		    downscalePlate_factor2_sse2_intr(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h, 0x80);
        }
        else {
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
        }
        else {
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
        }
        else {
            downscalePlate_factor8(dst->data[0], dst->linesize[0], src->data[0], src_width, src->linesize[0], src->height);
            downscalePlate_factor8(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
            downscalePlate_factor8(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
        }
    }
}

void CLVideoDecoderOutput::downscalePlate_factor2(unsigned char* dst, int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height)
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

void CLVideoDecoderOutput::downscalePlate_factor4(unsigned char* dst,  int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height)
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


void CLVideoDecoderOutput::downscalePlate_factor8(unsigned char* dst,  int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height)
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

bool CLVideoDecoderOutput::imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff)
{
    if (img1->width!=img2->width || img1->height!=img2->height || img1->format != img2->format) 
        return false;

    if (!equalPlanes(img1->data[0], img2->data[0], img1->width, img1->linesize[0], img2->linesize[0], img1->height, max_diff))
        return false;

    int uv_h = img1->format == PIX_FMT_YUV420P ? img1->height/2 : img1->height;

    if (!equalPlanes(img1->data[1], img2->data[1], img1->width/2, img1->linesize[1], img2->linesize[1], uv_h, max_diff))
        return false;

    if (!equalPlanes(img1->data[2], img2->data[2], img1->width/2, img1->linesize[2], img2->linesize[2], uv_h, max_diff))
        return false;

    return true;
}

bool CLVideoDecoderOutput::equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, 
                                       int stride1, int stride2, int height, int max_diff)
{
    const unsigned char* p1 = plane1;
    const unsigned char* p2 = plane2;

    int difs = 0;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (qAbs (*p1 - *p2) > max_diff )
            {
                ++difs;
                if (difs==2)
                    return false;

            }
            else
            {
                difs = 0;
            }

            ++p1;
            ++p2;
        }

        p1+=(stride1-width);
        p2+=(stride2-width);
    }

    return true;
}

void CLVideoDecoderOutput::saveToFile(const char* filename)
{

    static bool first_time  = true;
    FILE * out = 0;

    if (first_time)
    {
        out = fopen(filename, "wb");
        first_time = false;
    }
    else
        out = fopen(filename, "ab");

    if (!out) return;

    //Y
    unsigned char* p = data[0];
    for (int i = 0; i < height; ++i)
    {
        fwrite(p, 1, width, out);
        p+=linesize[0];
    }

    //U
    p = data[1];
    for (int i = 0; i < height/2; ++i)
    {
        fwrite(p, 1, width/2, out);
        p+=linesize[1];
    }

    //V
    p = data[2];
    for (int i = 0; i < height/2; ++i)
    {
        fwrite(p, 1, width/2, out);
        p+=linesize[2];
    }

}

bool CLVideoDecoderOutput::isPixelFormatSupported(PixelFormat format)
{
    return format == PIX_FMT_YUV422P || format == PIX_FMT_YUV420P || format == PIX_FMT_YUV444P;
}
