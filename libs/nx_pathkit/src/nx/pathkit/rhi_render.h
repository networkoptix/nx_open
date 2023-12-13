// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QCache>

#include "rhi_paint_engine.h"

class QRhi;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiSampler;
class QRhiRenderPassDescriptor;
class QRhiShaderResourceBindings;
class QRhiCommandBuffer;
class QRhiRenderTarget;
class QRhiResourceUpdateBatch;
class QRhiTexture;

namespace nx::pathkit {

class NX_PATHKIT_API RhiPaintDeviceRenderer
{
public:
    struct Batch
    {
        int offset;
        int count;

        std::unique_ptr<QRhiShaderResourceBindings> bindigs;
        QRhiGraphicsPipeline* pipeline = nullptr;
        QRhiBuffer* vertexInput = nullptr;
        std::unique_ptr<QRhiBuffer> dataBuffer;

        std::function<void(QRhi*, QRhiCommandBuffer*, QSize)> render;
    };

    struct TextureEntry
    {
        std::shared_ptr<QRhiTexture> texture;

        TextureEntry(std::shared_ptr<QRhiTexture> t): texture(t) {}
    };

public:
    RhiPaintDeviceRenderer(QRhi* rhi);
    ~RhiPaintDeviceRenderer();

    void sync(RhiPaintDevice* pd);

    void prepare(QRhiRenderPassDescriptor* rp, QRhiResourceUpdateBatch* u);
    void render(QRhiCommandBuffer* cb);

private:
    void createTexturePipeline(QRhiRenderPassDescriptor* rp);
    void createPathPipeline(QRhiRenderPassDescriptor* rp);

    QMatrix4x4 modelView() const;

    std::vector<Batch> processEntries(
        QRhiRenderPassDescriptor* rp,
        QRhiResourceUpdateBatch* u,
        std::vector<float>& triangles,
        std::vector<float>& colors,
        std::vector<float>& textureVerts,
        QSize clip);

    std::unique_ptr<QRhiShaderResourceBindings> createTextureBinding(
        QRhiResourceUpdateBatch* rub,
        const QPixmap& pixmap);

    QRhi* m_rhi;
    QSize m_size;
    std::vector<PaintData> entries;

    // Path fill/stroke (color) pipeline settings.
    std::unique_ptr<QRhiGraphicsPipeline> cps;
    std::unique_ptr<QRhiShaderResourceBindings> csrb;
    std::unique_ptr<QRhiBuffer> vbuf; //< Vertex.
    std::unique_ptr<QRhiBuffer> cbuf; //< Color.
    std::unique_ptr<QRhiBuffer> ubuf; //< Uniform.
    std::vector<std::unique_ptr<QRhiShaderResourceBindings>> pathBindings;

    // Pixmap (texture) pipeline settings.
    std::unique_ptr<QRhiGraphicsPipeline> tps;
    std::unique_ptr<QRhiBuffer> tubuf; //< Uniform
    std::unique_ptr<QRhiBuffer> tvbuf; //< Vertex + texture coordinates.
    std::unique_ptr<QRhiSampler> sampler;
    std::unique_ptr<QRhiShaderResourceBindings> tsrb;

    std::vector<Batch> batches;

    std::vector<std::shared_ptr<QRhiTexture>> textures;

    QCache<qint64, TextureEntry> m_textureCache;
};

} // namespace nx::pathkit
