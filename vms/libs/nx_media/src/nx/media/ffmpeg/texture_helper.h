// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory.h>

#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/QVideoFrameFormat>

extern "C" {
#include <libavutil/pixfmt.h>
} // extern "C"

struct AVFrame;

namespace nx::media {

NX_MEDIA_API AVPixelFormat getFormat(const AVFrame* frame);

std::unique_ptr<QRhiTexture> createTextureFromHandle(
    const QVideoFrameFormat& fmt,
    QRhi* rhi,
    int plane,
    quint64 handle,
    int layout = 0,
    QRhiTexture::Flags textureFlags = {});

QVideoFrameFormat::PixelFormat toQtPixelFormat(const AVFrame* frame);

} // nx::media
