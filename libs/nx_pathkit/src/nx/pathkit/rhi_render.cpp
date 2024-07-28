// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// Qt foreach macro breaks Skia code, so include Skia headers first.
#include <include/core/SkPathEffect.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/pathops/SkPathOps.h>
#include <src/gpu/ganesh/GrEagerVertexAllocator.h>
#include <src/gpu/ganesh/geometry/GrAATriangulator.h>
#include <src/gpu/ganesh/geometry/GrTriangulator.h>

#include "rhi_render.h"

#include <QtCore/QFile>
#include <QtGui/QImage>
#include <QtGui/rhi/qrhi.h>

#include <nx/utils/log/assert.h>
#include <nx/pathkit/rhi_paint_device.h>
#include <nx/pathkit/rhi_paint_engine.h>

namespace nx::pathkit {

class VertexAllocator: public GrEagerVertexAllocator
{
public:
    void* lock(size_t stride, int eagerCount) override
    {
        m_lastStride = stride;
        const size_t allocSize = m_size + eagerCount * stride;
        if (allocSize > m_capacity)
        {
            m_data.realloc(allocSize);
            m_capacity = allocSize;
        }
        return m_data + m_size;
    }

    void unlock(int actualCount) override
    {
        m_size += actualCount * m_lastStride;
    }

    size_t size() const
    {
        return m_size / sizeof(float);
    }

    float& operator[](int i)
    {
        return *reinterpret_cast<float*>(m_data + i * sizeof(float));
    }

    float operator[](int i) const
    {
        return *reinterpret_cast<const float*>(m_data + i * sizeof(float));
    }

    const void* data() const
    {
        return m_data;
    }

private:
    skia_private::AutoTMalloc<char> m_data;
    size_t m_capacity = 0;
    size_t m_size = 0;
    size_t m_lastStride = 0;
};

namespace {

static constexpr float kTolerance = 0.25f;

static const QRhiShaderResourceBinding::StageFlags kCommonVisibility =
    QRhiShaderResourceBinding::VertexStage
    | QRhiShaderResourceBinding::FragmentStage;

int pathToTriangles(
    const SkPath& path,
    QColor color,
    const nx::pathkit::PaintPath& pp,
    VertexAllocator& triangles,
    VertexAllocator& colors,
    QSize clip)
{
    const auto prevSize = triangles.size();
    const SkRect clipBounds = SkRect::MakeXYWH(0, 0, clip.width(), clip.height());
    bool isLinear;
    if (pp.aa)
        GrAATriangulator::PathToAATriangles(path, kTolerance, clipBounds, &triangles);
    else
        GrTriangulator::PathToTriangles(path, kTolerance, clipBounds, &triangles, &isLinear);

    // Each triangle vertex is 3 floats: x, y, alpha.
    const auto numVerts = (triangles.size() - prevSize) / 3;

    const float colorF[4] = {
        color.redF(),
        color.greenF(),
        color.blueF(),
        (float) pp.opacity * color.alphaF()};

    float* pColor = (float*) colors.lock(sizeof(float), numVerts * 4);

    // Add color for each vertex.
    for (size_t i = 0; i < numVerts; ++i)
    {
        memmove(pColor, colorF, 4 * sizeof(float));
        pColor += 4;
    }

    colors.unlock(numVerts * 4);

    return numVerts;
}

SkPaint::Cap getSkCap(Qt::PenCapStyle style)
{
    switch (style)
    {
        case Qt::FlatCap: return SkPaint::kButt_Cap;
        case Qt::SquareCap: return SkPaint::kSquare_Cap;
        case Qt::RoundCap: return SkPaint::kRound_Cap;
        default: return SkPaint::kDefault_Cap;
    }
}

SkPaint::Join getSkJoin(Qt::PenJoinStyle style)
{
    switch (style)
    {
        case Qt::MiterJoin: return SkPaint::kMiter_Join;
        case Qt::BevelJoin: return SkPaint::kBevel_Join;
        case Qt::RoundJoin: return SkPaint::kRound_Join;
        case Qt::SvgMiterJoin:
        case Qt::MPenJoinStyle:
        default: return SkPaint::kDefault_Join;
    }
}

sk_sp<SkPathEffect> getPenDashEffect(const QPen& pen)
{
    std::vector<float> intervals;

    switch (pen.style())
    {
        case Qt::CustomDashLine:
        {
            const auto pattern = pen.dashPattern();
            intervals.reserve(pattern.size() + pattern.size() % 2);

            for (auto value: pattern)
                intervals.push_back(value * pen.widthF());
            if (intervals.size() % 2) //< Skia requires even number.
                intervals.push_back(intervals.front());
            break;
        }
        case Qt::DashLine:
            for (auto value: {4, 2})
                intervals.push_back(value * pen.widthF());
            break;
        case Qt::DotLine:
            for (auto value: {1, 2})
                intervals.push_back(value * pen.widthF());
            break;
        case Qt::DashDotLine:
            for (auto value: {4, 2, 1, 2})
                intervals.push_back(value * pen.widthF());
            break;
        case Qt::DashDotDotLine:
            for (auto value: {4, 2, 1, 2, 1, 2})
                intervals.push_back(value * pen.widthF());
            break;
        case Qt::SolidLine:
        default:
            return nullptr;
    }

    return SkDashPathEffect::Make(
        intervals.data(),
        intervals.size(),
        pen.dashOffset() * pen.widthF());
}

void setupBlend(QRhiGraphicsPipeline* pipeline)
{
    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;

    premulAlphaBlend.enable = true;
    pipeline->setTargetBlends({ premulAlphaBlend });

    pipeline->setDepthTest(false);
}

} // namespace

static constexpr auto kMatrix4x4Size = 4 * 4 * sizeof(float);

RhiPaintDeviceRenderer::RhiPaintDeviceRenderer(QRhi* rhi, Settings settings):
    m_rhi(rhi),
    m_settings(settings)
{
    const int maxSize = m_rhi->resourceLimit(QRhi::TextureSizeMax);
    if (m_settings.cacheSize == 0)
        m_settings.cacheSize = maxSize;
}

RhiPaintDeviceRenderer::~RhiPaintDeviceRenderer() {}

void RhiPaintDeviceRenderer::sync(RhiPaintDevice* pd)
{
    m_size = {pd->width(), pd->height()};
    entries = static_cast<RhiPaintEngine*>(pd->paintEngine())->m_paths;
}

static constexpr int kPadding = 1;
static constexpr auto kFilter = QRhiSampler::Linear;

void RhiPaintDeviceRenderer::createTexturePipeline(QRhiRenderPassDescriptor* rp)
{
    #if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        // Allow uploading textures with QImage::Format_ARGB32_Premultiplied format.
        std::unique_ptr<QRhiTexture> tex(m_rhi->newTexture(
            QRhiTexture::BGRA8, {1, 1}, 1, {}));
        isBgraSupported = tex->create();
        if (!isBgraSupported)
        {
            tex->setFormat(QRhiTexture::RGBA8);
            tex->create();
        }
    #else
        std::unique_ptr<QRhiTexture> tex(m_rhi->newTexture(
            QRhiTexture::RGBA8, {1, 1}, 1, {}));
        tex->create();
    #endif

    tvbuf.reset(
        m_rhi->newBuffer(
            QRhiBuffer::Dynamic,
            QRhiBuffer::VertexBuffer,
            4 * 5 * sizeof(float)));
    tvbuf->create();

    std::unique_ptr<QRhiBuffer> tubuf(
        m_rhi->newBuffer(
            QRhiBuffer::Dynamic,
            QRhiBuffer::UniformBuffer,
            kMatrix4x4Size));
    tubuf->create();

    sampler.reset(m_rhi->newSampler(
        kFilter,
        kFilter,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge));
    sampler->create();

    tsrb.reset(m_rhi->newShaderResourceBindings());
    tsrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            kCommonVisibility,
            tubuf.get()),
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            tex.get(),
            sampler.get())
    });
    tsrb->create();

    tps.reset(m_rhi->newGraphicsPipeline());

    setupBlend(tps.get());

    static auto getShader = [](const QString &name) {
        QFile f(name);
        return NX_ASSERT(f.open(QIODevice::ReadOnly), "Unable to load shader %1", name)
            ? QShader::fromSerialized(f.readAll())
            : QShader();
    };
    tps->setShaderStages({
        { QRhiShaderStage::Vertex, getShader(":/nx_pathkit/shaders/texture.vert.qsb") },
        { QRhiShaderStage::Fragment, getShader(":/nx_pathkit/shaders/texture.frag.qsb") }
    });
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 5 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
        { 0, 2, QRhiVertexInputAttribute::Float, 4 * sizeof(float) },
    });
    tps->setTopology(QRhiGraphicsPipeline::Triangles);
    tps->setVertexInputLayout(inputLayout);
    tps->setShaderResourceBindings(tsrb.get());
    tps->setRenderPassDescriptor(rp);
    tps->create();

    m_textureCache.setMaxCost(m_settings.cacheSize * m_settings.cacheSize);

    const int atlasSize = std::min(
        m_rhi->resourceLimit(QRhi::TextureSizeMax),
        m_settings.atlasSize);

    atlasTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, QSize(atlasSize, atlasSize), 1));
    atlasTexture->create();
    atlasTsrb.reset(m_rhi->newShaderResourceBindings());
    atlasTsrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            kCommonVisibility,
            ubuf.get()),
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            atlasTexture.get(),
            sampler.get())
    });
    atlasTsrb->create();
    atlas.reset(new Atlas(atlasTexture->pixelSize().width(), atlasTexture->pixelSize().height()));
}

void RhiPaintDeviceRenderer::createPathPipeline(QRhiRenderPassDescriptor* rp)
{
    vbuf.reset(m_rhi->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::VertexBuffer,
        1));
    vbuf->create();

    cbuf.reset(m_rhi->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::VertexBuffer,
        1));
    cbuf->create();

    ubuf.reset(m_rhi->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::UniformBuffer,
        kMatrix4x4Size));
    ubuf->create();

    csrb.reset(m_rhi->newShaderResourceBindings());
    csrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0,
            kCommonVisibility,
            ubuf.get())
    });
    csrb->create();

    cps.reset(m_rhi->newGraphicsPipeline());

    setupBlend(cps.get());

    static auto getShader = [](const QString &name) {
        QFile f(name);
        return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
    };
    cps->setShaderStages({
        { QRhiShaderStage::Vertex, getShader(QLatin1String(":/nx_pathkit/shaders/color.vert.qsb")) },
        { QRhiShaderStage::Fragment, getShader(QLatin1String(":/nx_pathkit/shaders/color.frag.qsb")) }
    });
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 3 * sizeof(float) },
        { 4 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 1, 1, QRhiVertexInputAttribute::Float4, 0 },
    });
    cps->setVertexInputLayout(inputLayout);
    cps->setShaderResourceBindings(csrb.get());
    cps->setRenderPassDescriptor(rp);
    cps->create();
}

bool RhiPaintDeviceRenderer::prepare(QRhiRenderPassDescriptor* rp, QRhiResourceUpdateBatch* u)
{
    if (!cps)
    {
        createPathPipeline(rp);
        createTexturePipeline(rp);
    }

    VertexAllocator vertexData;
    VertexAllocator colors;
    std::vector<float> textureData;

    batches = processEntries(
        rp,
        u,
        vertexData,
        colors,
        textureData,
        m_size);

    const auto resizeBuffer =
        [&](QRhiBuffer* buffer, size_t minSize)
        {
            if (buffer->size() < minSize * sizeof(float))
            {
                buffer->setSize(minSize * sizeof(float));
                return buffer->create();
            }
            return true;
        };

    if (!resizeBuffer(vbuf.get(), vertexData.size()))
        return false;
    if (!resizeBuffer(cbuf.get(), colors.size()))
        return false;
    if (!resizeBuffer(tvbuf.get(), textureData.size()))
        return false;

    u->updateDynamicBuffer(vbuf.get(), 0, vertexData.size() * sizeof(float), vertexData.data());
    u->updateDynamicBuffer(cbuf.get(), 0, colors.size() * sizeof(float), colors.data());
    u->updateDynamicBuffer(tvbuf.get(), 0, textureData.size() * sizeof(float), textureData.data());

    u->updateDynamicBuffer(ubuf.get(), 0, kMatrix4x4Size, modelView().constData());
    return true;
}

QMatrix4x4 RhiPaintDeviceRenderer::modelView() const
{
    QMatrix4x4 viewProjection = m_rhi->clipSpaceCorrMatrix();
    viewProjection.translate(-1, 1, 0);
    viewProjection.scale(2.0 / m_size.width(), -2.0 / m_size.height());
    return viewProjection;
}

QRectF RhiPaintDeviceRenderer::textureCoordsFromAtlas(const Atlas::Rect& rect, int padding) const
{
    if (padding == 1)
    {
        return QRectF(
            (float) (rect.x + padding) / atlas->width(),
            (float) (rect.y + padding)/ atlas->height(),
            (float) (rect.w - 2 * padding) / atlas->width(),
            (float) (rect.h - 2 * padding) / atlas->height());
    }

    return QRectF(
        (float) (rect.x + 0.5) / atlas->width(),
        (float) (rect.y + 0.5) / atlas->height(),
        (float) (rect.w - 1) / atlas->width(),
        (float) (rect.h - 1) / atlas->height());
}

QImage RhiPaintDeviceRenderer::getImage(const QPixmap& pixmap)
{
    QImage image = pixmap.toImage();

    if (image.format() == QImage::Format_RGBA8888_Premultiplied)
        return image;

    if (isBgraSupported && image.format() == QImage::Format_ARGB32_Premultiplied)
        return image;

    image.convertTo(QImage::Format_RGBA8888_Premultiplied);
    return image;
}

QRhiShaderResourceBindings* RhiPaintDeviceRenderer::createTextureBinding(
    QRhiResourceUpdateBatch* rub,
    const QPixmap& pixmap,
    QRectF* outRect)
{
    const auto key = pixmap.cacheKey();

    const int maxSize = m_settings.maxAtlasEntrySize;

    if (pixmap.size().width() <= maxSize && pixmap.size().height() <= maxSize
        && !m_textureCache.contains(key))
    {
        if (const auto rect = atlasCache.value(key); !rect.isNull())
        {
            *outRect = textureCoordsFromAtlas(rect, kPadding);
            return atlasTsrb.get();
        }
    }

    std::shared_ptr<QRhiTexture> texture;

    if (TextureEntry* cached = m_textureCache.object(key))
    {
        texture = cached->texture;
    }
    else
    {
        auto image = getImage(pixmap);
        const auto textureFormat = image.format() == QImage::Format_ARGB32_Premultiplied
            ? QRhiTexture::BGRA8
            : QRhiTexture::RGBA8;
        texture.reset(m_rhi->newTexture(textureFormat, pixmap.size(), 1));
        texture->create();
        rub->uploadTexture(texture.get(), image);
        const auto size = texture->pixelSize();
        m_textureCache.insert(key, new TextureEntry(texture), size.width() * size.height());
    }

    std::unique_ptr<QRhiShaderResourceBindings> tsrb(m_rhi->newShaderResourceBindings());
    tsrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            kCommonVisibility,
            ubuf.get()),
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            texture.get(),
            sampler.get())
    });
    tsrb->create();

    textures.push_back({
        .texture = texture,
        .tsrb = std::move(tsrb)
    });

    *outRect = QRectF(0, 0, 1, 1);
    return textures.back().tsrb.get();
}

void RhiPaintDeviceRenderer::prepareAtlas()
{
    std::unordered_set<Atlas::Rect> usedRects;
    bool atlasContainsNewRectsOnly = false;

    std::vector<QPixmap> smallPixmaps;

    const int maxSize = m_settings.maxAtlasEntrySize;

    for (const auto& entry: entries)
    {
        if (auto paintPixmap = std::get_if<PaintPixmap>(&entry))
        {
            const auto& pixmap = paintPixmap->pixmap;
            const auto key = pixmap.cacheKey();
            if (pixmap.size().width() <= maxSize && pixmap.size().height() <= maxSize
                && !m_textureCache.contains(key))
            {
                if (const auto rect = atlasCache.value(key); !rect.isNull())
                    usedRects.insert(rect);
                else
                    smallPixmaps.push_back(pixmap);
            }
        }
    }

    std::sort(smallPixmaps.begin(), smallPixmaps.end(),
        [](const auto& p1, const auto& p2)
        {
            return p1.width() * p1.height() < p2.width() * p2.height();
        });

    for (const auto& pixmap: smallPixmaps)
    {
        auto rect = atlas->add(pixmap.width() + 2 * kPadding, pixmap.height() + 2 * kPadding);

        if (rect.isNull() && !atlasContainsNewRectsOnly)
        {
            // Not enough space, remove unused rects.
            // Do not remove rects that we are going to use in this frame. If we already did that,
            // just skip using the atlas as there are no rects we can safely remove.

            for (const auto& entry: entries)
            {
                if (auto paintPixmap = std::get_if<PaintPixmap>(&entry))
                {
                    auto rect = atlasCache.value(paintPixmap->pixmap.cacheKey());
                    if (!rect.isNull())
                        usedRects.insert(rect);
                }
            }

            atlas->removeIf([&usedRects](const auto& rect) { return !usedRects.contains(rect); });
            atlasCache.removeIf(
                [&usedRects](const std::pair<const quint64&, Atlas::Rect&>& keyAndRect)
                {
                    return !usedRects.contains(keyAndRect.second);
                });

            atlasContainsNewRectsOnly = true;

            rect = atlas->add(pixmap.width() + 2 * kPadding, pixmap.height() + 2 * kPadding);
        }

        if (!rect.isNull())
        {
            atlasCache.insert(pixmap.cacheKey(), rect);
            usedRects.insert(rect);

            QImage image;

            if (kPadding == 1)
            {
                // Render the pixmap into a larger image suitable for texture uploading.
                image = QImage(
                    pixmap.width() + kPadding * 2,
                    pixmap.height() + kPadding * 2,
                    QImage::Format_RGBA8888_Premultiplied);
                image.fill(Qt::transparent);
                image.setDevicePixelRatio(pixmap.devicePixelRatio());
                QPainter painter(&image);
                painter.drawPixmap(QPointF(kPadding, kPadding) / pixmap.devicePixelRatio(), pixmap);

                // Fill the paddings to avoid texture bleading.
                constexpr int bytesPerPixel = 4;
                const int imageWidth = image.width();
                const int imageHeight = image.height();
                // Top.
                memmove(
                    image.scanLine(0) + bytesPerPixel,
                    image.scanLine(1) + bytesPerPixel,
                    (imageWidth - 2) * bytesPerPixel);
                // Bottom.
                memmove(
                    image.scanLine(imageHeight - 1) + bytesPerPixel,
                    image.scanLine(imageHeight - 2) + bytesPerPixel,
                    (imageWidth - 2) * bytesPerPixel);

                for (int i = 0; i < imageHeight; ++i)
                {
                    auto line = reinterpret_cast<int32_t*>(image.scanLine(i));
                    // Left
                    line[0] = line[1];
                    // Right.
                    line += (imageWidth - 2);
                    line[1] = line[0];
                }
            }
            else
            {
                image = pixmap.toImage();
                if (image.format() != QImage::Format_RGBA8888_Premultiplied)
                    image.convertTo(QImage::Format_RGBA8888_Premultiplied);
            }

            NX_ASSERT(rect.w == image.width() && rect.h == image.height());

            QRhiTextureSubresourceUploadDescription subresDesc(image);
            subresDesc.setDestinationTopLeft(QPoint(rect.x, rect.y));
            QRhiTextureUploadEntry entry(0, 0, subresDesc);

            atlasUpdates.push_back(entry);
        }
    }
}

void RhiPaintDeviceRenderer::fillTextureVerts(
    const PaintPixmap& paintPixmap,
    const QRectF& textureSrc,
    QSize clip,
    std::vector<float>& textureVerts)
{
    const QRectF srcRect = paintPixmap.src.isEmpty()
        ? QRectF(QPoint(0, 0), paintPixmap.pixmap.size())
        : paintPixmap.src;

    QTransform toTexture;
    toTexture.translate(
        textureSrc.x(),
        textureSrc.y());
    toTexture.scale(
        textureSrc.width() / paintPixmap.pixmap.width(),
        textureSrc.height() / paintPixmap.pixmap.height());

    const auto dst = paintPixmap.dst;

    const QPointF topLeft = paintPixmap.transform.map(dst.topLeft());
    const QPointF topRight = paintPixmap.transform.map(dst.topRight());
    const QPointF bottomLeft = paintPixmap.transform.map(dst.bottomLeft());
    const QPointF bottomRight = paintPixmap.transform.map(dst.bottomRight());

    if (paintPixmap.clip)
    {
        SkPath borderPath;
        borderPath.moveTo(topLeft.x(), topLeft.y());
        borderPath.lineTo(topRight.x(), topRight.y());
        borderPath.lineTo(bottomRight.x(), bottomRight.y());
        borderPath.lineTo(bottomLeft.x(), bottomLeft.y());
        borderPath.close();

        SkPath clipped;
        Op(borderPath, *paintPixmap.clip, SkPathOp::kIntersect_SkPathOp, &clipped);

        VertexAllocator triangles;
        bool isLinear;
        GrTriangulator::PathToTriangles(
            clipped,
            kTolerance,
            /*clipBounds*/ SkRect::MakeXYWH(0, 0, clip.width(), clip.height()),
            &triangles,
            &isLinear);

        bool invertible = false;
        QTransform inverted = paintPixmap.transform.inverted(&invertible);

        QTransform toPixmap;
        toPixmap.translate(srcRect.x(), srcRect.y());
        toPixmap.scale(
            srcRect.width() / dst.width(),
            srcRect.height() / dst.height());
        toPixmap.translate(-dst.x(), -dst.y());

        inverted = inverted * toPixmap * toTexture;

        constexpr auto kStride = 3;
        textureVerts.reserve(textureVerts.size() + 5 * (triangles.size() / kStride));

        for (size_t i = 0; i + kStride <= triangles.size(); i += kStride)
        {
            const QPointF v(triangles[i], triangles[i + 1]);
            const QPointF t = inverted.map(v);

            textureVerts.push_back(v.x());
            textureVerts.push_back(v.y());
            textureVerts.push_back(t.x());
            textureVerts.push_back(t.y());
            textureVerts.push_back(paintPixmap.opacity);
        }
    }
    else
    {
        const auto rect = toTexture.mapRect(srcRect);

        const qreal vx[4] = {topLeft.x(), topRight.x(), bottomRight.x(), bottomLeft.x()};
        const qreal vy[4] = {topLeft.y(), topRight.y(), bottomRight.y(), bottomLeft.y()};
        const qreal tx[4] = {rect.left(), rect.right(), rect.right(), rect.left()};
        const qreal ty[4] = {rect.top(), rect.top(), rect.bottom(), rect.bottom()};

        const auto emitVertex =
            [&vx, &vy, &tx, &ty, &textureVerts, opacity=paintPixmap.opacity](int i)
            {
                textureVerts.push_back(vx[i]);
                textureVerts.push_back(vy[i]);
                textureVerts.push_back(tx[i]);
                textureVerts.push_back(ty[i]);
                textureVerts.push_back(opacity);
            };

        textureVerts.reserve(textureVerts.size() + 5 * (3 + 3));

        // 0 - 1
        // | / |
        // 3 - 2
        for (int i: {0, 1, 3, 3, 1, 2})
            emitVertex(i);
    }
}

std::vector<RhiPaintDeviceRenderer::Batch> RhiPaintDeviceRenderer::processEntries(
    QRhiRenderPassDescriptor* rp,
    QRhiResourceUpdateBatch* u,
    VertexAllocator& triangles,
    VertexAllocator& colors,
    std::vector<float>& textureVerts,
    QSize clip)
{
    std::vector<RhiPaintDeviceRenderer::Batch> batches;

    textures.clear();
    atlasUpdates.clear();

    prepareAtlas();

    for (const auto& entry: entries)
    {
        if (auto paintPath = std::get_if<PaintPath>(&entry))
        {
            const auto addPath =
                [&](const SkPath& path, QColor color)
                {
                    int offset = triangles.size();
                    int colorsOffset = colors.size();

                    if (pathToTriangles(path, color, *paintPath, triangles, colors, clip) == 0)
                        return;

                    if (!batches.empty() && batches.back().colorInput)
                    {
                        batches.back().count += ((int) triangles.size() - offset) / 3;
                        return;
                    }

                    batches.push_back({
                        .offset = offset,
                        .colorsOffset = colorsOffset,
                        .count = ((int) triangles.size() - offset) / 3,
                        .bindigs = csrb.get(),
                        .pipeline = cps.get(),
                        .vertexInput = vbuf.get(),
                        .colorInput = cbuf.get(),
                    });
                };

            if (paintPath->brush.style() != Qt::NoBrush)
            {
                if (paintPath->clip)
                {
                    SkPath clipped;
                    Op(paintPath->path, *paintPath->clip, SkPathOp::kIntersect_SkPathOp, &clipped);
                    addPath(paintPath->path, paintPath->brush.color());
                }
                else
                {
                    addPath(paintPath->path, paintPath->brush.color());
                }
            }

            if (paintPath->pen.style() != Qt::NoPen)
            {
                SkStrokeRec strokeRec(SkStrokeRec::kHairline_InitStyle);
                strokeRec.setStrokeStyle(paintPath->pen.widthF(), /* strokeAndFill */ false);
                strokeRec.setStrokeParams(
                    getSkCap(paintPath->pen.capStyle()),
                    getSkJoin(paintPath->pen.joinStyle()),
                    paintPath->pen.miterLimit());
                SkPath tmp;

                if (auto dashEffect = getPenDashEffect(paintPath->pen))
                {
                    dashEffect->filterPath(&tmp, paintPath->path, &strokeRec, nullptr);
                    strokeRec.applyToPath(&tmp, tmp);
                }
                else
                {
                    strokeRec.applyToPath(&tmp, paintPath->path);
                }

                if (paintPath->clip)
                    Op(tmp, *paintPath->clip, SkPathOp::kIntersect_SkPathOp, &tmp);

                addPath(tmp, paintPath->pen.color());
            }
        }
        else if (auto paintPixmap = std::get_if<PaintPixmap>(&entry))
        {
            QRectF src;
            auto bindings = createTextureBinding(u, paintPixmap->pixmap, &src);

            const auto offset = textureVerts.size();

            fillTextureVerts(*paintPixmap, src, clip, textureVerts);

            if (!batches.empty() && batches.back().bindigs == bindings)
            {
                batches.back().count += (textureVerts.size() - offset) / 5;
            }
            else
            {
                batches.push_back({
                    .offset = (int) offset,
                    .count = (int) (textureVerts.size() - offset) / 5,
                    .bindigs = bindings,
                    .pipeline = tps.get(),
                    .vertexInput = tvbuf.get(),
                });
            }
        }
        else if (auto customPaint = std::get_if<PaintCustom>(&entry))
        {
            customPaint->prepare(m_rhi, rp, u, modelView());
            batches.push_back({.render = customPaint->render});
        }
    }

    if (!atlasUpdates.empty())
    {
        QRhiTextureUploadDescription desc;
        desc.setEntries(atlasUpdates.begin(), atlasUpdates.end());
        u->uploadTexture(atlasTexture.get(), desc);
        atlasUpdates.clear();
    }

    return batches;
}

void RhiPaintDeviceRenderer::render(QRhiCommandBuffer* cb)
{
    for (const auto& batch: batches)
    {
        if (batch.render)
        {
            batch.render(m_rhi, cb, m_size);
            continue;
        }

        cb->setGraphicsPipeline(batch.pipeline);
        cb->setViewport(QRhiViewport(0, 0, m_size.width(), m_size.height()));
        cb->setShaderResources(batch.bindigs);

        if (batch.colorInput)
        {
            const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                { batch.vertexInput, batch.offset * sizeof(float) },
                { batch.colorInput, batch.colorsOffset * sizeof(float) }
            };
            cb->setVertexInput(0, 2, vbufBindings);
        }
        else
        {
            const QRhiCommandBuffer::VertexInput vbufBinding(
                batch.vertexInput,
                batch.offset * sizeof(float));
            cb->setVertexInput(0, 1, &vbufBinding);
        }
        cb->draw(batch.count);
    }
}

} // namespace nx::pathkit
