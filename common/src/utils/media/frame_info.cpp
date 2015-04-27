#include "frame_info.h"

#include <cstring>
#include <cstdio>

#include <utils/math/math.h>

extern "C" {
#ifdef WIN32
#   define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#ifdef WIN32
#   undef AVPixFmtDescriptor
#endif
};


/////////////////////////////////////////////////////
//  class QnSysMemPictureData
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
//  class QnOpenGLPictureData
/////////////////////////////////////////////////////
QnOpenGLPictureData::QnOpenGLPictureData(
    SynchronizationContext* const syncCtx,
//	GLXContext _glContext,
	unsigned int _glTexture )
:
    QnAbstractPictureDataRef( syncCtx ),
//	m_glContext( _glContext ),
	m_glTexture( _glTexture )
{
}

//!Returns context of texture
//GLXContext QnOpenGLPictureData::glContext() const
//{
//    return m_glContext;
//}

//!Returns OGL texture name
unsigned int QnOpenGLPictureData::glTexture() const
{
    return m_glTexture;
}


/////////////////////////////////////////////////////
//  class CLVideoDecoderOutput
/////////////////////////////////////////////////////
CLVideoDecoderOutput::CLVideoDecoderOutput()
{
    memset( this, 0, sizeof(*this) );
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
        dst->reallocate(src->width, src->height, src->format);

    dst->assignMiscData(src);
    //TODO/IMPL
    //dst->metadata = QnMetaDataV1Ptr( new QnMetaDataV1( *src->metadata ) );

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[src->format];
    for (int i = 0; i < descr->nb_components && src->data[i]; ++i)
    {
        int h = src->height;
        int w = src->width;
        if (i > 0) {
            h >>= descr->log2_chroma_h;
            w >>= descr->log2_chroma_w;
        }
        copyPlane(dst->data[i], src->data[i], w, dst->linesize[i], src->linesize[i], h);
    }
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

void CLVideoDecoderOutput::memZerro()
{
    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[format];
    for (int i = 0; i < descr->nb_components && data[i]; ++i)
    {
        int w = linesize[i];
        int h = height;
        if (i > 0)
            h >>= descr->log2_chroma_h;

        int bpp = descr->comp[i].step_minus1 + 1;
        int fillLen = h*w*bpp;
        memset(data[i], i == 0 ? 0 : 0x80, fillLen);
    }
}

void CLVideoDecoderOutput::reallocate(const QSize& size, int newFormat)
{
    reallocate(size.width(), size.height(), newFormat);
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
        numBytes += FF_INPUT_BUFFER_PADDING_SIZE; // extra alloc space due to ffmpeg doc
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

void CLVideoDecoderOutput::copyDataFrom(const AVFrame* frame)
{
    Q_ASSERT(width == frame->width);
    Q_ASSERT(height == frame->height);
    Q_ASSERT(format == frame->format);

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[format];
    for (int i = 0; i < descr->nb_components && frame->data[i]; ++i)
    {
        int h = height;
        int w = width;
        if (i > 0) {
            h >>= descr->log2_chroma_h;
            w >>= descr->log2_chroma_w;
        }
        copyPlane(data[i], frame->data[i], w, linesize[i], frame->linesize[i], h);
    }
}


CLVideoDecoderOutput::CLVideoDecoderOutput(QImage image) 
{
    memset( this, 0, sizeof(*this) );

    reallocate(image.width(), image.height(), PIX_FMT_YUV420P);
    CLVideoDecoderOutput src;

    src.reallocate(width, height, PIX_FMT_BGRA);
    for (int y = 0; y < height; ++y)
        memcpy(src.data[0] + src.linesize[0]*y, image.scanLine(y), width * 4);

    SwsContext* scaleContext = sws_getContext(width, height, PIX_FMT_BGRA, 
                                              width, height, PIX_FMT_YUV420P, 
                                              SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(scaleContext, src.data, src.linesize, 0, height, data, linesize);
    sws_freeContext(scaleContext);
}

QImage CLVideoDecoderOutput::toImage() const
{
    CLVideoDecoderOutput dst;
    dst.reallocate(width, height, PIX_FMT_BGRA);

    SwsContext* scaleContext = sws_getContext(width, height, (PixelFormat) format, 
                                              width, height, PIX_FMT_BGRA,
                                              SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(scaleContext, data, linesize, 0, height, dst.data, dst.linesize);
    sws_freeContext(scaleContext);

    QImage img(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y)
        memcpy(img.scanLine(y), dst.data[0] + dst.linesize[0]*y, width * 4);
    
    return img;
}

void CLVideoDecoderOutput::assignMiscData(const CLVideoDecoderOutput* other)
{
    pkt_dts = other->pkt_dts;
    pkt_pts = other->pkt_pts;
    pts = other->pts;
    flags = other->flags;
    sample_aspect_ratio = other->sample_aspect_ratio;
    channel = other->channel;
}

CLVideoDecoderOutput* CLVideoDecoderOutput::scaled(const QSize& newSize, PixelFormat newFormat)
{
    if (newFormat == PIX_FMT_NONE)
        newFormat = (PixelFormat) format;
    CLVideoDecoderOutput* dst(new CLVideoDecoderOutput);
    dst->reallocate(newSize.width(), newSize.height(), newFormat);
    dst->assignMiscData(this);
    dst->sample_aspect_ratio = 1.0;

    SwsContext* scaleContext = sws_getContext(
        width, height, (PixelFormat) format, 
        newSize.width(), newSize.height(), newFormat, 
        SWS_BICUBIC, NULL, NULL, NULL);
    
    sws_scale(scaleContext, data, linesize, 0, height, dst->data, dst->linesize);
    sws_freeContext(scaleContext);
    return dst;
}

CLVideoDecoderOutput* CLVideoDecoderOutput::rotated(int angle)
{
    if (angle > 180)
        angle = 270;
    else if (angle > 90)
        angle = 180;
    else
        angle = 90;

    int dstWidth = width;
    int dstHeight = height;
    if (angle != 180)
        qSwap(dstWidth, dstHeight);

    bool transposeChroma = false;
    if (angle == 90 || angle == 270) {
        if (format == PIX_FMT_YUV422P || format == PIX_FMT_YUVJ422P)
            transposeChroma = true;
    }

    CLVideoDecoderOutput* dstPict(new CLVideoDecoderOutput());
    dstPict->reallocate(dstWidth, dstHeight, format);
    dstPict->assignMiscData(this);

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[format];
    for (int i = 0; i < descr->nb_components && data[i]; ++i) 
    {
        int filler = (i == 0 ? 0x0 : 0x80);
        int numButes = dstPict->linesize[i] * dstHeight;
        if (i > 0)
            numButes >>= descr->log2_chroma_h;
        memset(dstPict->data[i], filler, numButes);

        int w = width;
        int h = height;
        
        if (i > 0 && !transposeChroma) {
            w >>= descr->log2_chroma_w;
            h >>= descr->log2_chroma_h;
        }

        if (angle == 90)
        {
            int dstLineStep = dstPict->linesize[i];

            if (transposeChroma && i > 0)
            {
                for (int y = 0; y < h; y += 2) 
                {
                    quint8* src = data[i] + linesize[i] * y;
                    quint8* dst = dstPict->data[i] + (h - y)/2 - 1;
                    for (int x = 0; x < w/2; ++x) {
                        quint8 pixel = ((quint16) src[0] + (quint16) src[linesize[i]]) >> 1;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        src++;
                    }
                }
            }
            else {
                for (int y = 0; y < h; ++y) 
                {
                    quint8* src = data[i] + linesize[i] * y;
                    quint8* dst = dstPict->data[i] + h - y -1;
                    if (angle == 270)
                        dst += (w-1) * dstPict->linesize[i];
                    for (int x = 0; x < w; ++x) {
                        *dst = *src++;
                        dst += dstLineStep;
                    }
                }
            }
        }
        else if (angle == 270)
        {
            int dstLineStep = -dstPict->linesize[i];

            if (transposeChroma && i > 0)
            {
                for (int y = 0; y < h; y += 2) 
                {
                    quint8* src = data[i] + linesize[i] * y;
                    quint8* dst = dstPict->data[i] + (w-1) * dstPict->linesize[i] + y/2;
                    for (int x = 0; x < w/2; ++x) {
                        quint8 pixel = ((quint16) src[0] + (quint16) src[linesize[i]]) >> 1;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        dst[0] = pixel;
                        dst += dstLineStep;
                        src++;
                    }
                }
            }
            else {
                for (int y = 0; y < h; ++y) 
                {
                    quint8* src = data[i] + linesize[i] * y;
                    quint8* dst = dstPict->data[i] + (w-1) * dstPict->linesize[i] + y;
                    for (int x = 0; x < w; ++x) {
                        *dst = *src++;
                        dst += dstLineStep;
                    }
                }
            }
        }
        else 
        {  // 180
            for (int y = 0; y < h; ++y) {
                quint8* src = data[i] + linesize[i] * y;
                quint8* dst = dstPict->data[i] + dstPict->linesize[i] * (h-1 - y) + w-1;
                for (int x = 0; x < w; ++x) {
                    *dst-- = *src++;
                }
            }
        }
    }

    return dstPict;
}
