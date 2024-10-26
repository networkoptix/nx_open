// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_set>
#include <vector>

#include <QtCore/QCache>
#include <QtGui/rhi/qrhi.h>

#include "atlas.h"
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

// A wrapper for QGradient to get its cached color texture. Only stops are necessary for comparison.
class GradientWrapper
{
public:
    GradientWrapper(const QGradient& gradient): gradient(gradient) {}

    bool operator==(const GradientWrapper& other) const
    {
        return gradient.stops() == other.gradient.stops();
    }

    bool operator!=(const GradientWrapper& other) const
    {
        return !(*this == other);
    }

    const QGradient& getGradient() const
    {
        return gradient;
    }

private:
    QGradient gradient;
};

} // namespace nx::pathkit

template<>
struct std::hash<nx::pathkit::GradientWrapper>
{
    size_t operator()(const nx::pathkit::GradientWrapper& gradient) const noexcept
    {
        size_t hashValue = 0;

        for (const auto& stop: gradient.getGradient().stops())
        {
            hashValue = (hashValue * 17) ^ std::hash<qreal>{}(stop.first);
            hashValue = (hashValue * 17) ^ stop.second.rgba64();
        }

        return hashValue;
    }
};

namespace nx::pathkit {

class VertexAllocator;

class NX_PATHKIT_API RhiPaintDeviceRenderer
{
public:
    struct Batch
    {
        int offset = 0;
        int colorsOffset = 0;
        int count = 0;

        QRhiShaderResourceBindings* bindigs = nullptr;
        QRhiGraphicsPipeline* pipeline = nullptr;
        QRhiBuffer* vertexInput = nullptr;
        QRhiBuffer* colorInput = nullptr;

        std::function<void(QRhi*, QRhiCommandBuffer*, QSize)> render;
    };

    struct TextureEntry
    {
        std::shared_ptr<QRhiTexture> texture;

        TextureEntry(std::shared_ptr<QRhiTexture> t): texture(t) {}
    };

    struct Settings
    {
        int cacheSize = 0; //< Use max texture size.
        int atlasSize = 1024;
        int maxAtlasEntrySize = 128;
    };

    static constexpr Settings kDefaultSettinigs =
    {
        .cacheSize = 0,
        .atlasSize = 1024,
        .maxAtlasEntrySize = 128
    };

    struct Stats
    {
        size_t entries = 0;
        size_t batches = 0;
        size_t texturesInCache = 0;
        size_t texturesInAtlas = 0;
    };

public:
    RhiPaintDeviceRenderer(QRhi* rhi, Settings settings = kDefaultSettinigs);
    ~RhiPaintDeviceRenderer();

    void sync(RhiPaintDevice* pd);

    bool prepare(QRhiRenderPassDescriptor* rp, QRhiResourceUpdateBatch* u);
    void render(QRhiCommandBuffer* cb);

    Stats stats() const { return currentStats; }

private:
    void createTexturePipeline(QRhiRenderPassDescriptor* rp);
    void createPathPipeline(QRhiRenderPassDescriptor* rp);

    QMatrix4x4 modelView() const;

    std::vector<Batch> processEntries(
        const RhiPaintEngineSyncData::Entries& entries,
        QRhiRenderPassDescriptor* rp,
        QRhiResourceUpdateBatch* u,
        VertexAllocator& triangles,
        VertexAllocator& colors,
        std::vector<float>& textureVerts,
        QSize clip);

    QRhiShaderResourceBindings* getTextureBindings(
        QRhiResourceUpdateBatch* rub,
        const QPixmap& pixmap,
        QRectF* outRect);

    QRhiShaderResourceBindings* getTextureBindings(
        std::shared_ptr<QRhiTexture> texture,
        QRectF* outRect);

    std::shared_ptr<QRhiTexture> textureForGradient(
        QRhiResourceUpdateBatch* rub,
        const QGradient& gradient);

    QRectF textureCoordsFromAtlas(const Atlas::Rect& rect, int padding) const;

    void prepareAtlas(const RhiPaintEngineSyncData::Entries& entries);

    QImage getImage(const QPixmap& pixmap);
    void fillTextureVerts(
        const PaintPixmap& paintPixmap,
        const QRectF& textureSrc,
        QSize clip,
        std::vector<float>& textureVerts);

    QRhi* const m_rhi;
    Settings m_settings;
    QSize m_size;
    std::shared_ptr<RhiPaintEngineSyncData> inputData;
    bool isBgraSupported = false;
    std::unique_ptr<SkPathRefAllocator> pathAllocator;

    // Path fill/stroke (color) pipeline settings.
    std::unique_ptr<QRhiGraphicsPipeline> cps;
    std::unique_ptr<QRhiShaderResourceBindings> csrb;
    std::unique_ptr<QRhiBuffer> vbuf; //< Vertex.
    std::unique_ptr<QRhiBuffer> cbuf; //< Color.
    std::unique_ptr<QRhiBuffer> ubuf; //< Uniform.

    // Pixmap (texture) pipeline settings.
    std::unique_ptr<QRhiGraphicsPipeline> tps;
    std::unique_ptr<QRhiBuffer> tubuf; //< Uniform
    std::unique_ptr<QRhiBuffer> tvbuf; //< Vertex + texture coordinates.
    std::unique_ptr<QRhiSampler> sampler;
    std::unique_ptr<QRhiShaderResourceBindings> tsrb;

    std::unique_ptr<Atlas> atlas;
    std::unique_ptr<QRhiTexture> atlasTexture;
    std::vector<QRhiTextureUploadEntry> atlasUpdates;
    std::unique_ptr<QRhiShaderResourceBindings> atlasTsrb;
    QHash<quint64, Atlas::Rect> atlasCache;

    struct Texture
    {
        std::shared_ptr<QRhiTexture> texture;
        std::unique_ptr<QRhiShaderResourceBindings> tsrb;
    };
    std::vector<Texture> textures;

    QCache<qint64, TextureEntry> m_textureCache;
    QCache<GradientWrapper, TextureEntry> m_gradientCache;

    std::vector<Batch> batches;
    Stats currentStats;
};

} // namespace nx::pathkit
