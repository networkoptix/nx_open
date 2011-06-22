#include "frame_info.h"

#include <string.h>
#include <stdio.h>

// i am not sure about SSE syntax in 64-bit version, it is need testing. So, define is WIN32, not Q_OS_WIN
#ifdef WIN32

#ifdef Q_CC_MSVC
#define USED_U64(foo) static volatile const unsigned long long foo
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
#define USED_U64(foo) \
    static const quint64 foo __asm__ (#foo) __attribute__((used))
#else
#define USED_U64(foo) \
    static const uint64_t foo __asm__ (#foo) __attribute__((used))
#endif

USED_U64(mmx_00ffw)       = 0x00ff00ff00ff00ffull;
USED_U64(mmx_000000ffw)   = 0x000000ff000000ffull;
USED_U64(mmx_word_const_2)= 0x0002000200020002ull;


const quint16 __declspec(align(16)) sse_00ffw[]        = { 0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff,0x00ff };
const quint16 __declspec(align(16)) sse_word_const_2[] = { 0x0002,0x0002,0x0002,0x0002,0x0002,0x0002,0x0002,0x0002 };

void downscalePlate_factor2_sse(unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
                                const unsigned int width, const unsigned int src_stride, unsigned int height)
{
    unsigned int y = 0;
    int round_width = width / 32 * 32;
    height = height / 2 * 2;
    do {
        const unsigned char* src_line1 = src + src_stride * y;
        const unsigned char* src_line2 = src_line1 + src_stride;
        const unsigned char* src_line1_end = src_line1 + round_width;
        __asm 
        {
            mov esi, [src_line1]
            mov edx, [src_line2]
            mov edi, [dst]
            mov eax, [src_line1_end]
            .align 8;
			MOVAPS xmm6, sse_00ffw 
			MOVAPS xmm7, sse_word_const_2
__loop1:
            PREFETCHNTA [esi + 64]
            PREFETCHNTA [edx + 64]
			
            MOVUPS    xmm0, [esi]
            MOVUPS    xmm4, [esi+16]
            MOVUPS    xmm1, [edx]
            MOVUPS    xmm5, [edx+16]
            MOVAPS    xmm2, xmm0
            MOVAPS    xmm3, xmm1
            psrlw xmm2, 8
            psrlw xmm3, 8
            pand      xmm0, xmm6
            pand      xmm1, xmm6
            paddsw xmm0, xmm2
            paddsw xmm1, xmm3
            MOVAPS    xmm2, xmm4
            paddsw xmm0, xmm1
            MOVAPS    xmm3, xmm5
            psrlw xmm2, 8
            psrlw xmm3, 8
            pand      xmm4, xmm6
            pand      xmm5, xmm6
            paddsw xmm4, xmm2
            paddsw xmm5, xmm3
            paddsw    xmm0, xmm7
            paddsw xmm4, xmm5
            psrlw xmm0, 2 
            paddsw    xmm4, xmm7
            packuswb xmm0, xmm6
            psrlw xmm4, 2 
            packuswb xmm4, xmm6
            MOVLHPS xmm0, xmm4
            MOVNTPS [edi], xmm0

            add esi, 32
            add edx, 32
            add edi, 16
            cmp esi, eax
            jl __loop1
        }
        if (round_width != width) 
        {
            const unsigned char* src_line2_end = src_line2 + round_width;
            unsigned char * tmp_dst = dst + round_width / 2;
            for (int i = 0; i < width - round_width; i+=2)
            {
                *tmp_dst++ = ( (unsigned int)*src_line1_end + *(src_line1_end+2) + (unsigned int)*src_line2_end + *(src_line2_end+2) + 2 ) >> 2;
                src_line1_end += 2;
                src_line2_end += 2;
            }
        }

        y += 2;
        dst += dst_stride;
    } while ( y < height);
    __asm { emms }
}

void downscalePlate_factor4_sse(unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
                                const unsigned int width, const unsigned int src_stride, unsigned int height)
{
    unsigned int y = 0;
    int round_width = width / 32 * 32;
    height = height / 4 * 4;
    do {
        const unsigned char* src_line1 = src + src_stride * y;
        const unsigned char* src_line2 = src_line1 + src_stride*3;
        const unsigned char* src_line1_end = src_line1 + round_width;
        __asm 
        {
            mov esi, [src_line1]
            mov edx, [src_line2]
            mov edi, [dst]
            mov eax, [src_line1_end]
            .align 8;
			MOVAPS xmm6, sse_00ffw 
			MOVAPS xmm7, sse_word_const_2
__loop1:
            PREFETCHNTA [esi + 64]
            PREFETCHNTA [edx + 64]
			
            MOVUPS    xmm0, [esi]
            MOVUPS    xmm4, [esi+16]
            MOVUPS    xmm1, [edx]
            MOVUPS    xmm5, [edx+16]

            pand xmm0, xmm6
            pand xmm4, xmm6
            pand xmm1, xmm6
            pand xmm5, xmm6

            packuswb xmm0, xmm4
            packuswb xmm1, xmm5

            MOVAPS    xmm2, xmm0
            MOVAPS    xmm3, xmm1
            
            psrlw xmm2, 8
            psrlw xmm3, 8

            pand      xmm0, xmm6
            pand      xmm1, xmm6

            paddsw xmm0, xmm2
            paddsw xmm1, xmm3

            paddsw xmm0, xmm7
            paddsw    xmm0, xmm1

            psrlw xmm0, 2 
            packuswb xmm0, xmm6
            MOVLPS [edi], xmm0

            add esi, 32
            add edx, 32
            add edi, 8
            cmp esi, eax
            jl __loop1
        }
        if (round_width != width) 
        {
            const unsigned char* src_line2_end = src_line2 + round_width;
            unsigned char * tmp_dst = dst + round_width / 4;
            for (int i = 0; i < width - round_width; i+=4)
            {
                *tmp_dst++ = ( (unsigned int)*src_line1_end + *(src_line1_end+2) + (unsigned int)*src_line2_end + *(src_line2_end+2) + 2 ) >> 2;
                src_line1_end += 4;
                src_line2_end += 4;
            }
        }
        y += 4;
        dst += dst_stride;
    } while ( y < height);
    __asm { emms }
}

void downscalePlate_factor8_sse(unsigned char * dst, const unsigned int dst_stride, const unsigned char * src,
                                const unsigned int width, const unsigned int src_stride, unsigned int height)
{
    height = height / 8 * 8;
    int round_width = width / 32 * 32;
    unsigned int y = 0;
    do {
        const unsigned char* src_line1 = src + src_stride * y;
        const unsigned char* src_line2 = src_line1 + src_stride*7;
        const unsigned char* src_line1_end = src_line1 + round_width;
        __asm 
        {
            mov esi, [src_line1]
            mov edx, [src_line2]
            mov edi, [dst]
            mov eax, [src_line1_end]
            .align 8;
            movq mm7, mmx_word_const_2 // round value
            movq mm6, mmx_00ffw
__loop1:
            //PREFETCHNTA [esi + 64]
            //PREFETCHNTA [edx + 64]

            PINSRW mm0, [esi], 0
            PINSRW mm1, [esi+4], 0
            PINSRW mm0, [esi+8], 1
            PINSRW mm1, [esi+12], 1
            PINSRW mm0, [esi+16], 2
            PINSRW mm1, [esi+20], 2
            PINSRW mm0, [esi+24], 3
            PINSRW mm1, [esi+28], 3
            PINSRW mm2, [edx], 0
            PINSRW mm3, [edx+4], 0
            PINSRW mm2, [edx+8], 1
            PINSRW mm3, [edx+12], 1
            PINSRW mm2, [edx+16], 2
            PINSRW mm3, [edx+20], 2
            PINSRW mm2, [edx+24], 3
            PINSRW mm3, [edx+28], 3
            pand mm0, mm6
            pand mm1, mm6
            pand mm2, mm6
            pand mm3, mm6
            paddsw    mm0, mm1 ; add all pixels
            paddsw    mm2, mm3
            paddsw    mm0, mm7
            paddsw    mm0, mm2
            psrlw mm0, 2 // div by 4 pixel color
            packuswb mm0, mm0
            MOVD [edi], mm0

            add esi, 32
            add edx, 32
            add edi, 4
            cmp esi, eax
            jl __loop1
        }
        if (round_width != width) 
        {
            const unsigned char* src_line2_end = src_line2 + round_width;
            unsigned char * tmp_dst = dst + round_width / 8;
            for (int i = 0; i < width - round_width; i+=8)
            {
                *tmp_dst++ = ( (unsigned int)*src_line1_end + *(src_line1_end+4) + (unsigned int)*src_line2_end + *(src_line2_end+4) + 2 ) >> 2;
                src_line1_end += 8;
                src_line2_end += 8;
            }
        }
        y += 8;
        dst += dst_stride;
    } while ( y < height);
    __asm { emms }
}
#endif

#define q_abs(x) ((x) >= 0 ? (x) : (-x))

CLVideoDecoderOutput::CLVideoDecoderOutput():
m_capacity(0),
C1(0), C2(0), C3(0),
stride1(0),
width(0)
{

}

CLVideoDecoderOutput::~CLVideoDecoderOutput()
{
    if (m_capacity)
        clean();
}

void CLVideoDecoderOutput::clean()
{
    if (m_capacity)
    {
        av_free(C1);
        C1 = 0;
        m_capacity = 0;
    }
}

void CLVideoDecoderOutput::copy(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst)
{
    if (src->width != dst->width || src->height != dst->height || src->out_type != dst->out_type)
    {
        // need to realocate dst memory 
        dst->clean();
        dst->stride1 = src->width;
        dst->stride2 = src->width/2;
        dst->stride3 = src->width/2;

        dst->width = src->width;

        dst->height = src->height;
        dst->out_type = src->out_type;

        int yu_h = dst->out_type == PIX_FMT_YUV420P ? dst->height/2 : dst->height;

        dst->m_capacity = dst->stride1*dst->height + (dst->stride2+dst->stride3)*yu_h;

        dst->C1 = (unsigned char*) av_malloc(dst->m_capacity);
        dst->C2 = dst->C1 + dst->stride1*dst->height;
        dst->C3 = dst->C2 + dst->stride2*yu_h;

    }

    int yu_h = dst->out_type == PIX_FMT_YUV420P ? dst->height/2 : dst->height;

    copyPlane(dst->C1, src->C1, dst->stride1, dst->stride1, src->stride1, src->height);
    copyPlane(dst->C2, src->C2, dst->stride2, dst->stride2, src->stride2, yu_h);
    copyPlane(dst->C3, src->C3, dst->stride3, dst->stride3, src->stride3, yu_h);

}

void CLVideoDecoderOutput::copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride,  int src_stride, int height)
{

    for (int i = 0; i < height; ++i)
    {
        memcpy(dst, src, width);
        dst+=dst_stride;
        src+=src_stride;
    }
}

void CLVideoDecoderOutput::downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, downscale_factor factor)
{
    int src_width = src->width;
    int src_height = src->height;

    const int chroma_h_factor = (src->out_type == PIX_FMT_YUV420P || src->out_type == PIX_FMT_YUV422P) ? 2 : 1;
    const int chroma_v_factor = (src->out_type == PIX_FMT_YUV420P) ? 2 : 1;

    // after downscale chroma_width must be divisible by 4 ( opengl requirements )
    const int mod_w = chroma_h_factor*factor*4;
    if ((src_width%mod_w) != 0)
        src_width = src_width/mod_w*mod_w;

    dst->stride1 = src_width/factor;
    dst->stride2 = src_width/chroma_h_factor/factor;
    dst->stride3 = dst->stride2;

    dst->width = dst->stride1;

    dst->height = src_height/factor;
    dst->out_type = src->out_type;

    int yu_h = dst->height/chroma_v_factor;

    unsigned int new_min_capacity = dst->stride1*dst->height + (dst->stride2+dst->stride3)*yu_h;

    if (dst->m_capacity<new_min_capacity)
    {
        dst->clean();
        dst->m_capacity = new_min_capacity;
        dst->C1 = (unsigned char*) av_malloc(dst->m_capacity);
    }

    dst->C2 = dst->C1 + dst->stride1*dst->height;
    dst->C3 = dst->C2 + dst->stride2*yu_h;

    int src_yu_h = src_height/chroma_v_factor;

    if (factor == factor_2)
    {
#ifdef WIN32
		/*
		// perfomance test block
		// last test result: sandy bridge 4.3Ghz. SSE faster than c code at 4-4.5 times for 1920x1080 source
		QTime t1;
		t1.start();
		int g = 0;
		for (int i = 0; i < 1000; ++i)
		{
			downscalePlate_factor2_sse(dst->C1, src_width/2,   src->C1, src_width, src->stride1, src_height);
			downscalePlate_factor2_sse(dst->C2, src_width/chroma_h_factor/2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
			downscalePlate_factor2_sse(dst->C3, src_width/chroma_h_factor/2, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
			g+=i;
		}
		int e1 = t1.elapsed();

		QTime t2;
		t2.start();
		for (int i = 0; i < 1000; ++i)
		{
			downscalePlate_factor2(dst->C1, src->C1, src_width, src->stride1, src_height);
			downscalePlate_factor2(dst->C2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
			downscalePlate_factor2(dst->C3, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
			g+=i;
		}
		int e2 = t2.elapsed();
		cl_log.log("scale factor 2. c code time:", e2, cl_logALWAYS);
		cl_log.log("scale factor 2. sse time:", e1, cl_logALWAYS);
		cl_log.log("-------------------------",  g, cl_logALWAYS);
		*/
		downscalePlate_factor2_sse(dst->C1, src_width/2,   src->C1, src_width, src->stride1, src_height);
		downscalePlate_factor2_sse(dst->C2, src_width/chroma_h_factor/2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
		downscalePlate_factor2_sse(dst->C3, src_width/chroma_h_factor/2, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
#else
        downscalePlate_factor2(dst->C1, src->C1, src_width, src->stride1, src_height);
        downscalePlate_factor2(dst->C2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
        downscalePlate_factor2(dst->C3, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
#endif
    }
    else if(factor == factor_4)
    {
#ifdef WIN32
        downscalePlate_factor4_sse(dst->C1, src_width/4, src->C1, src_width, src->stride1, src->height);
        downscalePlate_factor4_sse(dst->C2, src_width/chroma_h_factor/4, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
        downscalePlate_factor4_sse(dst->C3, src_width/chroma_h_factor/4, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
#else
        downscalePlate_factor4(dst->C1, src->C1, src_width, src->stride1, src->height);
        downscalePlate_factor4(dst->C2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
        downscalePlate_factor4(dst->C3, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
#endif
    }
    else if(factor == factor_8)
    {
#ifdef WIN32
        downscalePlate_factor8_sse(dst->C1, src_width/8, src->C1, src_width, src->stride1, src->height);
        downscalePlate_factor8_sse(dst->C2, src_width/chroma_h_factor/8, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
        downscalePlate_factor8_sse(dst->C3, src_width/chroma_h_factor/8, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
#else
        downscalePlate_factor8(dst->C1, src->C1, src_width, src->stride1, src->height);
        downscalePlate_factor8(dst->C2, src->C2, src_width/chroma_h_factor, src->stride2, src_yu_h);
        downscalePlate_factor8(dst->C3, src->C3, src_width/chroma_h_factor, src->stride3, src_yu_h);
#endif
    }
}

void CLVideoDecoderOutput::downscalePlate_factor2(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height)
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

        src_line1-= src_width;
        src_line1+=(src_stride<<1);

        src_line2-= src_width;
        src_line2+=(src_stride<<1);
    }
}

void CLVideoDecoderOutput::downscalePlate_factor4(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height)
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

        src_line1-= src_width;
        src_line1+=(src_stride<<2);

        src_line2-= src_width;
        src_line2+=(src_stride<<2);
    }
}


void CLVideoDecoderOutput::downscalePlate_factor8(unsigned char* dst, const unsigned char* src, int src_width, int src_stride, int src_height)
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

        src_line1-= src_width;
        src_line1+=(src_stride<<3);

        src_line2-= src_width;
        src_line2+=(src_stride<<3);
    }
}

bool CLVideoDecoderOutput::imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff)
{
    if (img1->width!=img2->width || img1->height!=img2->height || img1->out_type != img2->out_type) 
        return false;

    if (!equalPlanes(img1->C1, img2->C1, img1->width, img1->stride1, img2->stride1, img1->height, max_diff))
        return false;

    int uv_h = img1->out_type == PIX_FMT_YUV420P ? img1->height/2 : img1->height;

    if (!equalPlanes(img1->C2, img2->C2, img1->width/2, img1->stride2, img2->stride2, uv_h, max_diff))
        return false;

    if (!equalPlanes(img1->C3, img2->C3, img1->width/2, img1->stride3, img2->stride3, uv_h, max_diff))
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
            if (q_abs (*p1 - *p2) > max_diff )
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
    unsigned char* p = C1;
    for (int i = 0; i < height; ++i)
    {
        fwrite(p, 1, width, out);
        p+=stride1;
    }

    //U
    p = C2;
    for (int i = 0; i < height/2; ++i)
    {
        fwrite(p, 1, width/2, out);
        p+=stride2;
    }

    //V
    p = C3;
    for (int i = 0; i < height/2; ++i)
    {
        fwrite(p, 1, width/2, out);
        p+=stride3;
    }

}
