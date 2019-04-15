#include "frame_info.h"

#include <nx/utils/log/log.h>

#ifdef ENABLE_DATA_PROVIDERS

#include <cstring>
#include <cstdio>
#include <array>
#include <memory>

#include <utils/math/math.h>

#include <nx/utils/switch.h>
#include <nx/utils/app_info.h>
#include <utils/color_space/yuvconvert.h>

#include <utils/media/ffmpeg_helper.h>

#include <nx/streaming/config.h>

extern "C" {
#ifdef WIN32
#   define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#ifdef WIN32
#   undef AVPixFmtDescriptor
#endif
};

/////////////////////////////////////////////////////
//  class QnOpenGLPictureData
/////////////////////////////////////////////////////
QnOpenGLPictureData::QnOpenGLPictureData(
    SynchronizationContext* const syncCtx,
//GLXContext _glContext,
    unsigned int _glTexture )
:
    QnAbstractPictureDataRef( syncCtx ),
//m_glContext( _glContext ),
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
    memset( this, 0, sizeof(AVFrame) );
}
CLVideoDecoderOutput::CLVideoDecoderOutput(int targetWidth, int targetHeight, int targetFormat):
    CLVideoDecoderOutput()
{
    reallocate(targetWidth, targetHeight, targetFormat);
}

CLVideoDecoderOutput::~CLVideoDecoderOutput()
{
    clean();
}

static bool convertImageFormat(
    int width,
    int height,
    const uint8_t* const sourceSlice[],
    const int sourceStride[],
    AVPixelFormat sourceFormat,
    uint8_t* const targetSlice[],
    const int targetStride[],
    AVPixelFormat targetFormat,
    const nx::utils::log::Tag& logTag)
{
    if (const auto context = sws_getContext(
        width, height, sourceFormat,
        width, height, targetFormat,
        SWS_BILINEAR, /*srcFilter*/ nullptr, /*dstFilter*/ nullptr, /*param*/nullptr))
    {
        sws_scale(context, sourceSlice, sourceStride, 0, height, targetSlice, targetStride);
        sws_freeContext(context);
        return true;
    }

    NX_ERROR(logTag, "Unable to convert video frame from %1 to %2: sws_getContext() failed.",
        sourceFormat, targetFormat);
    return false;
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

    const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) src->format);
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
    int yu_h = format == AV_PIX_FMT_YUV420P ? height/2 : height;
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
    const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) format);
    if (descr == nullptr)
        return;
    quint32 filler = 0;
    int w = width;
    int h = height;
    for (int i = 0; i < descr->nb_components && data[i]; ++i)
    {
        int bpp = descr->comp[i].step_minus1 + 1;
        int fillLen = linesize[i] - w*bpp;
        if (fillLen >= 4)
        {
            quint8* dst = data[i] + w*bpp;
            for (int y = 0; y < h; ++y)
            {
                *dst = filler;
                dst += linesize[i];
            }
        }
        if (i == 0)
        {
            filler = 0x80808080;
            w >>= descr->log2_chroma_w;
            h >>= descr->log2_chroma_h;
        }
    }
}

AVPixelFormat CLVideoDecoderOutput::fixDeprecatedPixelFormat(AVPixelFormat original)
{
    switch (original)
    {
        case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
        default: return original;
    }
}

void CLVideoDecoderOutput::memZero()
{
    const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) format);
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

    int rc = 32 >> (newFormat == AV_PIX_FMT_RGBA || newFormat == AV_PIX_FMT_ABGR || newFormat == AV_PIX_FMT_BGRA ? 2 : 0);
    int roundWidth = qPower2Ceil((unsigned) width, rc);
    int numBytes = av_image_get_buffer_size((AVPixelFormat) format, roundWidth, height, /*align*/ 1);
    if (numBytes > 0) {
        numBytes += FF_INPUT_BUFFER_PADDING_SIZE; // extra alloc space due to ffmpeg doc
        av_image_fill_arrays(
            data,
            linesize,
            (quint8*) av_malloc(numBytes),
            (AVPixelFormat) format, roundWidth, height, /*align*/ 1);
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

    int numBytes = av_image_get_buffer_size((AVPixelFormat) format, lineSizeHint, height, /*align*/ 1);
    if (numBytes > 0)
    {
        av_image_fill_arrays(
            data,
            linesize,
            (quint8*) av_malloc(numBytes),
            (AVPixelFormat) format, lineSizeHint, height, /*align*/ 1);
        fillRightEdge();
    }
}

bool CLVideoDecoderOutput::imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff)
{
    if (img1->width!=img2->width || img1->height!=img2->height || img1->format != img2->format)
        return false;

    if (!equalPlanes(img1->data[0], img2->data[0], img1->width, img1->linesize[0], img2->linesize[0], img1->height, max_diff))
        return false;

    int uv_h = img1->format == AV_PIX_FMT_YUV420P ? img1->height/2 : img1->height;

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

#if 0
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
#endif

bool CLVideoDecoderOutput::isPixelFormatSupported(AVPixelFormat format)
{
    return format == AV_PIX_FMT_YUV422P || format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV444P;
}

void CLVideoDecoderOutput::copyDataFrom(const AVFrame* frame)
{
    NX_ASSERT(width == frame->width);
    NX_ASSERT(height == frame->height);
    NX_ASSERT(fixDeprecatedPixelFormat((AVPixelFormat) format)
        == fixDeprecatedPixelFormat((AVPixelFormat) frame->format));

    if (const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) format))
    {
        for (int i = 0; i < descr->nb_components && frame->data[i]; ++i)
        {
            int h = height;
            int w = width;
            if (i > 0) {
                h >>= descr->log2_chroma_h;
                w >>= descr->log2_chroma_w;
            }
            copyPlane(data[i], frame->data[i], w * descr->comp[i].step, linesize[i], frame->linesize[i], h);
        }
    }
}

CLVideoDecoderOutput::CLVideoDecoderOutput(QImage image)
{
    memset( this, 0, sizeof(AVFrame) );

    reallocate(image.width(), image.height(), AV_PIX_FMT_YUV420P);
    CLVideoDecoderOutput src;

    src.reallocate(width, height, AV_PIX_FMT_BGRA);
    for (int y = 0; y < height; ++y)
        memcpy(src.data[0] + src.linesize[0]*y, image.scanLine(y), width * 4);

    // Ignore the potential error (already logged) - no clear way to handle it here.
    convertImageFormat(width, height,
        src.data, src.linesize, AV_PIX_FMT_BGRA,
        data, linesize, AV_PIX_FMT_YUV420P, /*logTag*/ this);
}

QImage CLVideoDecoderOutput::toImage() const
{
    // TODO: Investigate if the below mentioned bug is still present.
    // TODO: Consider using SSE via yuvconverter.h.

    // In some cases direct conversion to AV_PIX_FMT_BGRA causes crash in sws_scale
    // function. As workaround we can temporary convert to AV_PIX_FMT_ARGB format and then convert
    // to target AV_PIX_FMT_BGRA.
    const AVPixelFormat intermediateFormat = AV_PIX_FMT_ARGB;
    const AVPixelFormat targetFormat = AV_PIX_FMT_BGRA;

    if (width == 0 || height == 0)
        return QImage();

    CLVideoDecoderOutputPtr target(new CLVideoDecoderOutput(width, height, intermediateFormat));

    if (!convertImageFormat(width, height,
        data, linesize, (AVPixelFormat)format,
        target->data, target->linesize, intermediateFormat, /*logTag*/ this))
    {
        return {};
    }

    const CLVideoDecoderOutputPtr converted(new CLVideoDecoderOutput(width, height, targetFormat));
    if (!convertImageFormat(width, height,
        target->data, target->linesize, intermediateFormat,
        converted->data, converted->linesize, targetFormat, /*logTag*/ this))
    {
        return {};
    }

    target = converted;

    QImage img(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y)
        memcpy(img.scanLine(y), target->data[0] + target->linesize[0] * y, width * 4);

    return img;
}


/**
 * Convert the frame using the optimized conversion, if it is available for the source and
 * destination pixel formats.
 *
 * @param dstAvFrame Should be properly allocated according to dstAvPixelFormat.
 * @return Whether the optimized conversion was available.
 */
bool CLVideoDecoderOutput::convertUsingSimdIntrTo(
    AVPixelFormat dstAvPixelFormat, const AVFrame* dstAvFrame) const
{
    // Currently, the optimized conversion is not supported on arms (neon).
    #if !defined(__i386) && !defined(__amd64) && !defined(_WIN32)
        return false;
    #endif

    // NOTE: Despite the names of ..._argb32_simd_intr() converters, they actually convert
    // to BGRA (instead of ARGB as their names may suggest).
    if (dstAvPixelFormat != AV_PIX_FMT_BGRA)
        return false;

    const auto convertFrame = //< Pointer to the appropriate conversion function.
        nx::utils::switch_(format,
            AV_PIX_FMT_YUV420P, []{ return yuv420_argb32_simd_intr; },
            AV_PIX_FMT_YUV422P, []{ return yuv422_argb32_simd_intr; },
            AV_PIX_FMT_YUV444P, []{ return yuv444_argb32_simd_intr; },
            nx::utils::default_, []{ return nullptr; }
        );
    if (!convertFrame)
        return false;

    if (!NX_ASSERT(linesize[1] == linesize[2], "Src frame U and V planes have different strides.")
        || !NX_ASSERT(dstAvFrame->linesize[0] >= width * 4)
        || !NX_ASSERT(dstAvFrame->linesize[0] % CL_MEDIA_ALIGNMENT == 0))
    {
        return false;
    }

    convertFrame(
        dstAvFrame->data[0],
        data[0], data[1], data[2],
        width, height,
        /*dst_stride*/ dstAvFrame->linesize[0],
        /*y_stride*/ linesize[0],
        /*uv_stride*/ linesize[1],
        /*alpha*/ 255);

    return true;
}

std::shared_ptr<const AvFrameHolder> CLVideoDecoderOutput::convertTo(AVPixelFormat dstPixelFormat) const
{
    if (!NX_ASSERT(format >= 0))
        return nullptr;

    if (format == dstPixelFormat)
        return std::make_shared<const AvFrameHolder>(this);

    const auto frame = std::make_shared<AvFrameHolder>();

    const int r = av_image_alloc(
        frame->avFrame()->data,
		frame->avFrame()->linesize,
		width,
		height,
		dstPixelFormat,
		CL_MEDIA_ALIGNMENT);
    if (!NX_ASSERT(r > 0, lm("av_image_alloc() failed for pixel format %1").args(dstPixelFormat)))
        return nullptr;

    if (convertUsingSimdIntrTo(dstPixelFormat, frame->avFrame()))
        return frame;

    if (convertImageFormat(
        width, height,
        data, linesize, (AVPixelFormat) format,
        frame->avFrame()->data, frame->avFrame()->linesize, dstPixelFormat, /*logTag*/ this))
    {
        return frame;
    }

    return nullptr;
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

bool CLVideoDecoderOutput::invalidScaleParameters(const QSize& size) const
{
    return size.width() == 0 || size.height() == 0 || height == 0 || width == 0;
}

CLVideoDecoderOutput* CLVideoDecoderOutput::scaled(const QSize& newSize, AVPixelFormat newFormat) const
{
    if (invalidScaleParameters(newSize))
        return nullptr;

    if (newFormat == AV_PIX_FMT_NONE)
        newFormat = (AVPixelFormat)format;

    // Absolute robustness for the case, when operator 'new' throws an exception.
    std::unique_ptr<SwsContext, decltype(&sws_freeContext)> scaleContext(
        sws_getContext(width, height, (AVPixelFormat)format,
            newSize.width(), newSize.height(), newFormat,
            SWS_BICUBIC, NULL, NULL, NULL), sws_freeContext);

    if (scaleContext)
    {
        CLVideoDecoderOutput* dst(new CLVideoDecoderOutput);
        dst->reallocate(newSize.width(), newSize.height(), newFormat);
        dst->assignMiscData(this);
        dst->sample_aspect_ratio = 1.0;

        sws_scale(scaleContext.get(), data, linesize, 0, height, dst->data, dst->linesize);
        return dst;
    }

    return nullptr;
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
        if (format == AV_PIX_FMT_YUV422P || format == AV_PIX_FMT_YUVJ422P)
            transposeChroma = true;
    }

    CLVideoDecoderOutput* dstPict(new CLVideoDecoderOutput());
    dstPict->reallocate(dstWidth, dstHeight, format);
    dstPict->assignMiscData(this);

    const AVPixFmtDescriptor* descr = av_pix_fmt_desc_get((AVPixelFormat) format);
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

#endif // ENABLE_DATA_PROVIDERS
