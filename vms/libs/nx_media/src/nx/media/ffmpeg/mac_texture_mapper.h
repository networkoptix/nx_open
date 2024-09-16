// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <CoreVideo/CVMetalTexture.h>
#include <CoreVideo/CVMetalTextureCache.h>

#include <QtCore/qglobal.h>
#include <QtGui/rhi/qrhi.h>

#include <QtMultimedia/QVideoFrameFormat>

class QVideoFrameTextures;
class MacTextureSet;

class MacTextureMapper
{
public:
    virtual ~MacTextureMapper() = default;
    static std::unique_ptr<MacTextureMapper> create(CVImageBufferRef buffer, QRhi* rhi);
    virtual std::unique_ptr<QVideoFrameTextures> mapTextures(QRhi* rhi) = 0;
};
