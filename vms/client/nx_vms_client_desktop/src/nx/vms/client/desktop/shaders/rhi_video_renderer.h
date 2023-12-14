// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <utils/media/frame_info.h>

#include "media_output_shader_data.h"

class QRhi;
class QRhiRenderPassDescriptor;
class QRhiTexture;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;
class QRhiTextureRenderTarget;

namespace nx::vms::client::desktop {

/**
 * Renders CLConstVideoDecoderOutputPtr (with additional parameters) to QRhiCommandBuffer.
 */
class RhiVideoRenderer
{
public:
    struct Data
    {
        CLConstVideoDecoderOutputPtr frame;
        MediaOutputShaderData data;
        QRectF sourceRect;
    };

public:
    RhiVideoRenderer();
    ~RhiVideoRenderer();

    void prepare(
        const RhiVideoRenderer::Data& data,
        const QMatrix4x4& mvp,
        QRhi* rhi,
        QRhiRenderPassDescriptor* rp,
        QRhiResourceUpdateBatch* rub);

    void render(
        QRhiCommandBuffer* cb,
        QSize viewportSize);

private:
    void init(
        const RhiVideoRenderer::Data& data,
        QRhi* rhi,
        QRhiRenderPassDescriptor* desc);
    void createBindings();
    void ensureTextures();

    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
