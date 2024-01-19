// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "frame_info.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

#include <QtGui/QImage>

#include <nx/media/config.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/utils/switch.h>
#include <utils/color_space/yuvconvert.h>

extern "C" {
#ifdef WIN32
    #define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
#ifdef WIN32
    #undef AVPixFmtDescriptor
#endif
};

CLVideoDecoderOutput::CLVideoDecoderOutput():
    AVFrame()
{}

CLVideoDecoderOutput::CLVideoDecoderOutput(int targetWidth, int targetHeight, int targetFormat):
    CLVideoDecoderOutput()
{
    reallocate(targetWidth, targetHeight, targetFormat);
}

CLVideoDecoderOutput::~CLVideoDecoderOutput()
{
    clean();
}

bool CLVideoDecoderOutput::isEmpty() const
{
    return (m_memoryType == MemoryType::SystemMemory && !data[0]) ||
        (m_memoryType == MemoryType::VideoMemory && !m_surface);
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

void CLVideoDecoderOutput::attachVideoSurface(std::unique_ptr<AbstractVideoSurface> surface)
{
    m_memoryType = MemoryType::VideoMemory;
    m_surface = std::move(surface);
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
    {
        if (buf[0])
            av_frame_unref(this); //< Ref-counted frame.
        else
            av_free(data[0]); //< Manually allocated frame.
    }
    data[0] = data[1] = data[2] = 0;
    linesize[0] = linesize[1] = linesize[2] = 0;
    width = height = 0;
    m_memoryType = MemoryType::SystemMemory;
    m_surface.reset();
}

void CLVideoDecoderOutput::copyFrom(const CLVideoDecoderOutput* src)
{
    if (src->memoryType() == MemoryType::VideoMemory)
    {
        if (!src->getVideoSurface())
        {
            NX_WARNING(this, "Invalid video frame");
            return;
        }
        AVFrame lockedFrame = src->getVideoSurface()->lockFrame();
        if (!lockedFrame.data[0])
        {
            NX_WARNING(this, "Failed to lock video memory to copy data");
            return;
        }
        reallocate(lockedFrame.width, lockedFrame.height, AV_PIX_FMT_YUV420P);
        bool status = convertImageFormat(
            lockedFrame.width,
            lockedFrame.height,
            lockedFrame.data,
            lockedFrame.linesize,
            AVPixelFormat(lockedFrame.format),
            data,
            linesize,
            AV_PIX_FMT_YUV420P,
            /*logTag*/ this);
        src->getVideoSurface()->unlockFrame();
        if (!status)
        {
            NX_WARNING(this, "Failed to convert video memory color space to YUV420");
            return;
        }
    }
    else
    {
        copyDataOnlyFrom(src);
    }

    assignMiscData(src);
}

void CLVideoDecoderOutput::copyDataOnlyFrom(const AVFrame* src)
{
    if (src->width <=0 || src->height <= 0)
    {
        NX_ERROR(this, nx::format("Failed to copy frame, invalid frame resolution: %1x%2").args(
            src->width, src->height));
        return;
    }
    AVPixelFormat dstFormat = fixDeprecatedPixelFormat(AVPixelFormat(src->format));
    if (data[0] == nullptr || src->width != width || src->height != height || dstFormat != format)
        reallocate(src->width, src->height, dstFormat);

    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(dstFormat);
    if (!descriptor)
    {
        NX_ERROR(this, nx::format("Failed to copy frame, invalid pixel format: %1").arg(dstFormat));
        return;
    }
    for (int i = 0; i < QnFfmpegHelper::planeCount(descriptor) && src->data[i]; ++i)
    {
        int h = src->height;
        int w = src->width;
        if (QnFfmpegHelper::isChromaPlane(i, descriptor))
        {
            h >>= descriptor->log2_chroma_h;
            w >>= descriptor->log2_chroma_w;
        }
        const int bytes = (w * descriptor->comp[i].step * descriptor->comp[i].depth) / 8;
        copyPlane(data[i], src->data[i], bytes, linesize[i], src->linesize[i], h);
    }
}

void CLVideoDecoderOutput::copyPlane(
    uint8_t* dst, const uint8_t* src, int width, int dst_stride, int src_stride, int height)
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
    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get((AVPixelFormat) format);
    if (descriptor == nullptr)
        return;
    quint32 filler = 0;
    int w = width;
    int h = height;
    for (int i = 0; i < QnFfmpegHelper::planeCount(descriptor) && data[i]; ++i)
    {
        int bpp = descriptor->comp[i].step;
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
            w >>= descriptor->log2_chroma_w;
            h >>= descriptor->log2_chroma_h;
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
    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get((AVPixelFormat) format);
    for (int i = 0; i < QnFfmpegHelper::planeCount(descriptor) && data[i]; ++i)
    {
        int w = linesize[i];
        int h = height;
        if (QnFfmpegHelper::isChromaPlane(i, descriptor))
            h >>= descriptor->log2_chroma_h;

        int fillLen = h*w;
        memset(data[i], i == 0 ? 0 : 0x80, fillLen);
    }
}

int64_t CLVideoDecoderOutput::sizeBytes() const
{
    return av_image_get_buffer_size((AVPixelFormat)format, width, height, /*align*/ 1);
}

bool CLVideoDecoderOutput::reallocate(const QSize& size, int newFormat)
{
    return reallocate(size.width(), size.height(), newFormat);
}

bool CLVideoDecoderOutput::reallocate(int newWidth, int newHeight, int newFormat)
{
    clean();
    setUseExternalData(false);
    width = newWidth;
    height = newHeight;
    format = newFormat;

    int rc = 32 >> (newFormat == AV_PIX_FMT_RGBA || newFormat == AV_PIX_FMT_ABGR || newFormat == AV_PIX_FMT_BGRA ? 2 : 0);
    int roundWidth = qPower2Ceil((unsigned) width, rc);
    int numBytes = av_image_get_buffer_size((AVPixelFormat) format, roundWidth, height, /*align*/ 1);
    if (numBytes <= 0)
        return false;
    numBytes += AV_INPUT_BUFFER_PADDING_SIZE; // extra alloc space due to ffmpeg doc
    av_image_fill_arrays(
        data,
        linesize,
        (quint8*) av_malloc(numBytes),
        (AVPixelFormat) format, roundWidth, height, /*align*/ 1);
    fillRightEdge();
    return true;
}

bool CLVideoDecoderOutput::reallocate(int newWidth, int newHeight, int newFormat, int lineSizeHint)
{
    clean();
    setUseExternalData(false);
    width = newWidth;
    height = newHeight;
    format = newFormat;

    int numBytes = av_image_get_buffer_size((AVPixelFormat) format, lineSizeHint, height, /*align*/ 1);
    if (numBytes <= 0)
        return false;
    av_image_fill_arrays(
        data,
        linesize,
        (quint8*) av_malloc(numBytes),
        (AVPixelFormat) format, lineSizeHint, height, /*align*/ 1);
    fillRightEdge();
    return true;
}

/**
 * Intended for debug purposes.
 */
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

bool CLVideoDecoderOutput::isPixelFormatSupported(AVPixelFormat format)
{
    return format == AV_PIX_FMT_YUV422P || format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUV444P;
}

CLVideoDecoderOutput::CLVideoDecoderOutput(const QImage& image):
    AVFrame()
{
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

QByteArray CLVideoDecoderOutput::rawData() const
{
    QByteArray result;
    auto size = sizeBytes();
    if (size < 0)
    {
        NX_WARNING(this, "Failed to get the raw data, invalid pixel format: %1", format);
        return QByteArray();
    }

    result.resize(size);
    if (const auto r = av_image_copy_to_buffer(
        (uint8_t*) result.data(),
        size,
        data,
        linesize,
        (enum AVPixelFormat) format,
        width,
        height,
        /*align*/ 1);
        r < 0)
    {
        NX_WARNING(this, "Failed to get the raw data, FFmpeg error %1",
            QnFfmpegHelper::avErrorToString(r));
        return QByteArray();
    }
    return result;
}

QImage CLVideoDecoderOutput::toImage() const
{
    if (width == 0 || height == 0)
        return {};

    CLVideoDecoderOutput target(width, height, AV_PIX_FMT_RGB32);

    if (!convertImageFormat(width, height, data, linesize, static_cast<AVPixelFormat>(format),
        target.data, target.linesize, AV_PIX_FMT_RGB32, /*logTag*/ this))
    {
        return {};
    }

    QImage result(target.data[0], width, height, target.linesize[0], QImage::Format_RGB32,
        av_free, target.data[0]);

    target.data[0] = nullptr;

    return result;
}

/**
 * Convert the frame using the optimized conversion, if it is available for the source and
 * destination pixel formats.
 *
 * @param avFrame Should be properly allocated and have the same size.
 *
 * @return Whether the optimized conversion was available.
 */
bool CLVideoDecoderOutput::convertUsingSimdIntrTo(const AVFrame* avFrame) const
{
    // Currently, the optimized conversion is not supported on arms (neon).
    #if !defined(__i386) && !defined(__amd64) && !defined(_WIN32)
        return false;
    #endif

    // NOTE: Despite the names of ..._argb32_simd_intr() converters, they actually convert
    // to BGRA (instead of ARGB as their names may suggest).
    if (avFrame->format != AV_PIX_FMT_BGRA)
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

    if (!NX_ASSERT(linesize[1] == linesize[2],
            nx::format("Src frame U stride is %1 but V stride is %2").args(linesize[1], linesize[2]))
        || !NX_ASSERT(avFrame->linesize[0] >= width * 4)
        || !NX_ASSERT(avFrame->linesize[0] % CL_MEDIA_ALIGNMENT == 0))
    {
        return false;
    }

    convertFrame(
        avFrame->data[0],
        data[0], data[1], data[2],
        width, height,
        /*dst_stride*/ avFrame->linesize[0],
        /*y_stride*/ linesize[0],
        /*uv_stride*/ linesize[1],
        /*alpha*/ 255);

    return true;
}

bool CLVideoDecoderOutput::convertTo(const AVFrame* avFrame) const
{
    if (!NX_ASSERT(avFrame)
        || !NX_ASSERT(width == avFrame->width,
            nx::format("Src width is %1 but dst is %2").args(width, avFrame->width))
        || !NX_ASSERT(height == avFrame->height,
            nx::format("Src height is %1 but dst is %2").args(height, avFrame->height)))
    {
        return false;
    }

    if (convertUsingSimdIntrTo(avFrame))
        return true;

    return convertImageFormat(
        width, height,
        data, linesize, (AVPixelFormat) format,
        avFrame->data, avFrame->linesize, (AVPixelFormat) avFrame->format, /*logTag*/ this);
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

CLVideoDecoderOutputPtr CLVideoDecoderOutput::scaled(
    const QSize& newSize,
    AVPixelFormat newFormat,
    bool keepAspectRatio,
    int scaleFlags) const
{
    if (invalidScaleParameters(newSize))
        return nullptr;

    if (newFormat == AV_PIX_FMT_NONE)
    {
        newFormat = (AVPixelFormat)format;
        if (!sws_isSupportedOutput(newFormat))
            newFormat = AV_PIX_FMT_RGB24;
    }

    QSize scaleSize = newSize;
    if (keepAspectRatio)
    {
        const AVRational oldAspectRatio = {.num = width, .den = height};
        const AVRational newAspectRatio = {.num = newSize.width(), .den = newSize.height()};
        int comparationResult = av_cmp_q(oldAspectRatio, newAspectRatio);
        if (comparationResult > 0)
        {
            int64_t newHeight = newSize.width() * oldAspectRatio.den / oldAspectRatio.num;
            scaleSize.setHeight((int) newHeight);
        }
        else if (comparationResult < 0)
        {
            int64_t newWidth = newSize.height() * oldAspectRatio.num / oldAspectRatio.den;
            scaleSize.setWidth((int) newWidth);
        }
        NX_ASSERT(scaleSize.width() != 0 && scaleSize.width() <= newSize.width()
            && scaleSize.height() != 0 && scaleSize.height() <= newSize.height());
    }

    // Absolute robustness for the case, when operator 'new' throws an exception.
    std::unique_ptr<SwsContext, decltype(&sws_freeContext)> scaleContext(
        sws_getContext(width, height, (AVPixelFormat)format,
            scaleSize.width(), scaleSize.height(), newFormat,
            scaleFlags, NULL, NULL, NULL), sws_freeContext);

    if (scaleContext)
    {
        CLVideoDecoderOutputPtr dst(new CLVideoDecoderOutput);
        dst->reallocate(newSize.width(), newSize.height(), newFormat);
        dst->assignMiscData(this);
        dst->sample_aspect_ratio = 1.0;
        if (keepAspectRatio)
            dst->memZero();

        sws_scale(scaleContext.get(), data, linesize, 0, height, dst->data, dst->linesize);
        return dst;
    }

    return nullptr;
}
