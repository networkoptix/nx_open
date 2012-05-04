#include "frame_info.h"
#include "../common/util.h"

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
    int roundWidth = qPower2Ceil((unsigned) width, rc);
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
