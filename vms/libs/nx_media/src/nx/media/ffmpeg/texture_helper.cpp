// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "texture_helper.h"

#include <QtMultimedia/private/qvideotexturehelper_p.h>

#include <nx/build_info.h>
#include <nx/utils/log/log.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
} // extern "C"

namespace nx::media {

AVPixelFormat getFormat(const AVFrame* frame)
{
    if (!frame->hw_frames_ctx)
        return AVPixelFormat(frame->format);

    auto hwFramesContext = (AVHWFramesContext*)frame->hw_frames_ctx->data;

    return (AVPixelFormat) hwFramesContext->sw_format;
}

QVideoFrameFormat::PixelFormat toQtPixelFormat(const AVFrame* frame)
{
    switch (getFormat(frame))
    {
        case AV_PIX_FMT_RGBA:
            return QVideoFrameFormat::Format_RGBA8888;
        case AV_PIX_FMT_RGB0:
            return QVideoFrameFormat::Format_RGBX8888;
        case AV_PIX_FMT_NV12:
            return QVideoFrameFormat::Format_NV12;
        case AV_PIX_FMT_YUV422P:
            return QVideoFrameFormat::Format_YUV422P;
        case AV_PIX_FMT_YUV420P:
            return QVideoFrameFormat::Format_YUV420P;
        case AV_PIX_FMT_UYVY422:
            return QVideoFrameFormat::Format_UYVY;
        case AV_PIX_FMT_YUYV422:
            return QVideoFrameFormat::Format_YUYV;
        case AV_PIX_FMT_NONE:
        default:
            return QVideoFrameFormat::Format_Invalid;
    }
}

std::unique_ptr<QRhiTexture> createTextureFromHandle(
    const QVideoFrameFormat& fmt,
    QRhi* rhi,
    int plane,
    quint64 handle,
    int layout,
    QRhiTexture::Flags textureFlags)
{
    if (!handle)
        return {};

    const QVideoFrameFormat::PixelFormat pixelFormat = fmt.pixelFormat();
    const QSize size = fmt.frameSize();

    const auto texDesc = QVideoTextureHelper::textureDescription(fmt.pixelFormat());
    if (texDesc->sizeScale[plane].x == 0 || texDesc->sizeScale[plane].y == 0)
        return {};

    const QSize planeSize(
        size.width() / texDesc->sizeScale[plane].x,
        size.height() / texDesc->sizeScale[plane].y);

    if (pixelFormat == QVideoFrameFormat::Format_SamplerExternalOES)
    {
        if (nx::build_info::isAndroid() && rhi->backend() == QRhi::OpenGLES2)
            textureFlags |= QRhiTexture::ExternalOES;
    }
    else if (pixelFormat == QVideoFrameFormat::Format_SamplerRect)
    {
        if (nx::build_info::isMacOsX() && rhi->backend() == QRhi::OpenGLES2)
            textureFlags |= QRhiTexture::TextureRectangleGL;
    }

    if (texDesc->textureFormat[plane] == QRhiTexture::UnknownFormat)
        return {};

    std::unique_ptr<QRhiTexture> tex(
        rhi->newTexture(texDesc->textureFormat[plane], planeSize, 1, textureFlags));

    if (tex->createFrom({handle, layout}))
        return tex;

    NX_WARNING(NX_SCOPE_TAG, "Failed to initialize QRhiTexture wrapper for texture %1", handle);

    return {};
}

} // nx::media
