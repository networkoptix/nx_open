#include "frame_info.h"

#include <string.h>
#include <stdio.h>

// i am not sure about SSE syntax in 64-bit version, it is need testing. So, define is WIN32, not Q_OS_WIN
#ifdef WIN32

static const int ROUND_FACTOR = 32;

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
#else
static const int ROUND_FACTOR = 1;
#endif

#define q_abs(x) ((x) >= 0 ? (x) : (-x))

static int roundUp(int value)
{
    return value / ROUND_FACTOR * ROUND_FACTOR + (value % ROUND_FACTOR ? ROUND_FACTOR : 0);
}

CLVideoDecoderOutput::CLVideoDecoderOutput()
{
    /*
    width = 0;
    height = 0;
    interlaced_frame = 0;
    format = PIX_FMT_NONE;
    memset(data, 0, sizeof(data));
    memset(linesize, 0, sizeof(linesize));
    */
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

void CLVideoDecoderOutput::reallocate(int newWidth, int newHeight, int newFormat)
{
    clean();
    setUseExternalData(false);
    width = newWidth;
    height = newHeight;
    format = newFormat;

    int roundWidth = roundUp(width);
    int numBytes = avpicture_get_size((PixelFormat) format, roundWidth, height);
    avpicture_fill((AVPicture*) this, (quint8*) av_malloc(numBytes), (PixelFormat) format, roundWidth, height);
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

    //dst->linesize[0] = roundUp(src_width/factor);
    //dst->linesize[1] = dst->linesize[0] / chroma_h_factor;
    //dst->linesize[2] = dst->linesize[1];

    int scaledWidth = src_width/factor;
    int scaledHeight = src_height/factor;

    if (scaledWidth != dst->width || scaledHeight != dst->height || src->format != dst->format)
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
#ifdef WIN32
		/*
		// perfomance test block
		// last test result: sandy bridge 4.3Ghz. SSE faster than c code at 4-4.5 times for 1920x1080 source
		QTime t1;
		t1.start();
		int g = 0;
		for (int i = 0; i < 1000; ++i)
		{
			downscalePlate_factor2_sse(dst->data[0], src_width/2,   src->data[0], src_width, src->linesize[0], src_height);
			downscalePlate_factor2_sse(dst->data[1], src_width/chroma_h_factor/2, src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
			downscalePlate_factor2_sse(dst->data[2], src_width/chroma_h_factor/2, src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
			g+=i;
		}
		int e1 = t1.elapsed();

		QTime t2;
		t2.start();
		for (int i = 0; i < 1000; ++i)
		{
			downscalePlate_factor2(dst->data[0], src->data[0], src_width, src->linesize[0], src_height);
			downscalePlate_factor2(dst->data[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
			downscalePlate_factor2(dst->data[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
			g+=i;
		}
		int e2 = t2.elapsed();
		cl_log.log("scale factor 2. c code time:", e2, cl_logALWAYS);
		cl_log.log("scale factor 2. sse time:", e1, cl_logALWAYS);
		cl_log.log("-------------------------",  g, cl_logALWAYS);
		*/
		downscalePlate_factor2_sse(dst->data[0], dst->linesize[0],   src->data[0], src_width, src->linesize[0], src_height);
		downscalePlate_factor2_sse(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
		downscalePlate_factor2_sse(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
#else
        downscalePlate_factor2(dst->data[0], src->data[0], src_width, src->linesize[0], src_height);
        downscalePlate_factor2(dst->data[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
        downscalePlate_factor2(dst->data[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
#endif
    }
    else if(factor == factor_4)
    {
#ifdef WIN32
        downscalePlate_factor4_sse(dst->data[0], dst->linesize[0], src->data[0], src_width, src->linesize[0], src->height);
        downscalePlate_factor4_sse(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
        downscalePlate_factor4_sse(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
#else
        downscalePlate_factor4(dst->data[0], src->data[0], src_width, src->linesize[0], src->height);
        downscalePlate_factor4(dst->data[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
        downscalePlate_factor4(dst->data[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
#endif
    }
    else if(factor == factor_8)
    {
#ifdef WIN32
        downscalePlate_factor8_sse(dst->data[0], dst->linesize[0], src->data[0], src_width, src->linesize[0], src->height);
        downscalePlate_factor8_sse(dst->data[1], dst->linesize[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
        downscalePlate_factor8_sse(dst->data[2], dst->linesize[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
#else
        downscalePlate_factor8(dst->data[0], src->data[0], src_width, src->linesize[0], src->height);
        downscalePlate_factor8(dst->data[1], src->data[1], src_width/chroma_h_factor, src->linesize[1], src_yu_h);
        downscalePlate_factor8(dst->data[2], src->data[2], src_width/chroma_h_factor, src->linesize[2], src_yu_h);
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

