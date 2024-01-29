// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_render.h"

#include <QtCore/QFile>
#include <QtGui/QImage>

#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
    #include <rhi/qrhi.h>
#else
    #include <QtGui/private/qrhi_p.h>
#endif

#include <include/core/SkStrokeRec.h>
#include <include/pathops/SkPathOps.h>

#include <nx/utils/log/assert.h>
#include <nx/pathkit/rhi_paint_device.h>
#include <nx/pathkit/rhi_paint_engine.h>

using namespace pk;

namespace {

static const QRhiShaderResourceBinding::StageFlags kCommonVisibility =
    QRhiShaderResourceBinding::VertexStage
    | QRhiShaderResourceBinding::FragmentStage;

int pathToTriangles(
    const SkPath& path,
    QColor color,
    const nx::pathkit::PaintPath& pp,
    std::vector<float>& triangles,
    std::vector<float>& colors,
    QSize clip)
{
    const auto count = pp.aa
        ? path.toAATriangles(
            /*tolerance*/ 0.25,
            /*clipBounds*/ SkRect::MakeXYWH(0, 0, clip.width(), clip.height()),
            &triangles)
        : path.toTriangles(
            /*tolerance*/ 0.25,
            /*clipBounds*/ SkRect::MakeXYWH(0, 0, clip.width(), clip.height()),
            &triangles);

    if (count > 0)
    {
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(pp.opacity * color.alphaF());
    }

    return count;
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

void setupBlend(QRhiGraphicsPipeline* pipeline)
{
    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;

    premulAlphaBlend.enable = true;
    pipeline->setTargetBlends({ premulAlphaBlend });

    pipeline->setDepthTest(false);
}

} // namespace

namespace nx::pathkit {

RhiPaintDeviceRenderer::RhiPaintDeviceRenderer(QRhi* rhi): m_rhi(rhi) {}

RhiPaintDeviceRenderer::~RhiPaintDeviceRenderer() {}

void RhiPaintDeviceRenderer::sync(RhiPaintDevice* pd)
{
    m_size = {pd->width(), pd->height()};
    entries = static_cast<RhiPaintEngine*>(pd->paintEngine())->m_paths;
}

void RhiPaintDeviceRenderer::createTexturePipeline(QRhiRenderPassDescriptor* rp)
{
    std::unique_ptr<QRhiTexture> tex(m_rhi->newTexture(
        QRhiTexture::RGBA8, {1, 1}, 1, {}));
    tex->create();

    tvbuf.reset(m_rhi->newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::VertexBuffer,
        4 * 5 * sizeof(float)));
    tvbuf->create();

    std::unique_ptr<QRhiBuffer> tubuf(
        m_rhi->newBuffer(QRhiBuffer::Dynamic,
        QRhiBuffer::UniformBuffer,
        64 + 4));
    tubuf->create();

    sampler.reset(m_rhi->newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
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
    tps->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    tps->setVertexInputLayout(inputLayout);
    tps->setShaderResourceBindings(tsrb.get());
    tps->setRenderPassDescriptor(rp);
    tps->create();
}

void RhiPaintDeviceRenderer::createPathPipeline(QRhiRenderPassDescriptor* rp)
{
    vbuf.reset(m_rhi->newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::VertexBuffer,
        1));
    vbuf->create();

    cbuf.reset(m_rhi->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::UniformBuffer,
        4 * 4));
    cbuf->create();

    ubuf.reset(m_rhi->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::UniformBuffer,
        64 + 4));
    ubuf->create();

    csrb.reset(m_rhi->newShaderResourceBindings());
    csrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0,
            kCommonVisibility,
            ubuf.get()),
        QRhiShaderResourceBinding::uniformBuffer(
            1,
            QRhiShaderResourceBinding::VertexStage,
            cbuf.get())
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
        { 3 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
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

    std::vector<float> vertexData;
    std::vector<float> colors;
    std::vector<float> textureData;

    batches = processEntries(
        rp,
        u,
        vertexData,
        colors,
        textureData,
        m_size);

    vbuf->setSize(vertexData.size() * sizeof(vertexData[0]));
    if (!vbuf->create())
        return false;

    tvbuf->setSize(textureData.size() * sizeof(textureData[0]));
    if (!tvbuf->create())
        return false;

    u->uploadStaticBuffer(vbuf.get(), vertexData.data());
    u->uploadStaticBuffer(tvbuf.get(), textureData.data());

    u->updateDynamicBuffer(ubuf.get(), 0, 64, modelView().constData());
    return true;
}

QMatrix4x4 RhiPaintDeviceRenderer::modelView() const
{
    QMatrix4x4 viewProjection = m_rhi->clipSpaceCorrMatrix();
    viewProjection.translate(-1, 1, 0);
    viewProjection.scale(2.0 / m_size.width(), -2.0 / m_size.height());
    return viewProjection;
}

std::unique_ptr<QRhiShaderResourceBindings> RhiPaintDeviceRenderer::createTextureBinding(
    QRhiResourceUpdateBatch* rub,
    const QPixmap& pixmap)
{
    const auto key = pixmap.cacheKey();

    std::shared_ptr<QRhiTexture> texture;

    if (TextureEntry* cached = m_textureCache.object(key))
    {
        texture = cached->texture;
    }
    else
    {
        auto img = pixmap.toImage();
        img.convertTo(QImage::Format_RGBA8888_Premultiplied);

        texture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, pixmap.size(), 1));

        texture->create();
        rub->uploadTexture(texture.get(), img);
        m_textureCache.insert(key, new TextureEntry(texture));
    }

    textures.push_back(texture);

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
    return tsrb;
}

std::vector<RhiPaintDeviceRenderer::Batch> RhiPaintDeviceRenderer::processEntries(
    QRhiRenderPassDescriptor* rp,
    QRhiResourceUpdateBatch* u,
    std::vector<float>& triangles,
    std::vector<float>& colors,
    std::vector<float>& textureVerts,
    QSize clip)
{
    std::vector<RhiPaintDeviceRenderer::Batch> batches;

    textures.clear();

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

                    std::unique_ptr<QRhiBuffer> colorBuf(m_rhi->newBuffer(
                        QRhiBuffer::Dynamic,
                        QRhiBuffer::UniformBuffer,
                        4 * sizeof(float)));
                    colorBuf->create();

                    u->updateDynamicBuffer(colorBuf.get(), 0, colorBuf->size(), &colors[colorsOffset]);

                    std::unique_ptr<QRhiShaderResourceBindings> bindings(
                        m_rhi->newShaderResourceBindings());
                    bindings->setBindings({
                        QRhiShaderResourceBinding::uniformBuffer(
                            0,
                            kCommonVisibility,
                            ubuf.get()),
                        QRhiShaderResourceBinding::uniformBuffer(
                            1,
                            QRhiShaderResourceBinding::VertexStage,
                            colorBuf.get())
                    });
                    bindings->create();

                    batches.push_back({
                        .offset = offset,
                        .count = ((int) triangles.size() - offset) / 3,
                        .bindigs = std::move(bindings),
                        .pipeline = cps.get(),
                        .vertexInput = vbuf.get(),
                        .dataBuffer = std::move(colorBuf)
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
                SkPaint skPaint;
                skPaint.setStroke(true);
                skPaint.setStrokeCap(getSkCap(paintPath->pen.capStyle()));
                skPaint.setStrokeJoin(getSkJoin(paintPath->pen.joinStyle()));
                skPaint.setStrokeMiter(paintPath->pen.miterLimit());
                skPaint.setStrokeWidth(paintPath->pen.widthF());
                SkStrokeRec strokeRec(skPaint);
                SkPath tmp;
                strokeRec.applyToPath(&tmp, paintPath->path);

                if (paintPath->clip)
                    Op(tmp, *paintPath->clip, SkPathOp::kIntersect_SkPathOp, &tmp);

                addPath(tmp, paintPath->pen.color());
            }
        }
        else if (auto paintPixmap = std::get_if<PaintPixmap>(&entry))
        {
            auto r = paintPixmap->dst;

            const QPointF topLeft = paintPixmap->transform.map(r.topLeft());
            const QPointF topRight = paintPixmap->transform.map(r.topRight());
            const QPointF bottomLeft = paintPixmap->transform.map(r.bottomLeft());
            const QPointF bottomRight = paintPixmap->transform.map(r.bottomRight());

            QRectF src(0, 0, 1, 1);

            if (const auto s = paintPixmap->pixmap.size(); !s.isEmpty())
            {
                src.setX(paintPixmap->src.x() / s.width());
                src.setY(paintPixmap->src.y() / s.height());
                src.setWidth(paintPixmap->src.width() / s.width());
                src.setHeight(paintPixmap->src.height() / s.height());
            }

            std::vector<float> vertexData = {
                (float) topLeft.x(),     (float) topLeft.y(),     (float) src.left(),  (float) src.top(),    (float) paintPixmap->opacity,
                (float) topRight.x(),    (float) topRight.y(),    (float) src.right(), (float) src.top(),    (float) paintPixmap->opacity,
                (float) bottomLeft.x(),  (float) bottomLeft.y(),  (float) src.left(),  (float) src.bottom(), (float) paintPixmap->opacity,
                (float) bottomRight.x(), (float) bottomRight.y(), (float) src.right(), (float) src.bottom(), (float) paintPixmap->opacity,
            };

            batches.push_back({
                .offset = (int) textureVerts.size(),
                .count = 4,
                .bindigs = createTextureBinding(u, paintPixmap->pixmap),
                .pipeline = tps.get(),
                .vertexInput = tvbuf.get(),
            });

            std::copy(vertexData.begin(), vertexData.end(), std::back_inserter(textureVerts));
        }
        else if (auto customPaint = std::get_if<PaintCustom>(&entry))
        {
            customPaint->prepare(m_rhi, rp, u, modelView());
            batches.push_back({.render = customPaint->render});
        }
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
        cb->setShaderResources(batch.bindigs.get());
        const QRhiCommandBuffer::VertexInput vbufBinding(
            batch.vertexInput,
            batch.offset * sizeof(float));
        cb->setVertexInput(0, 1, &vbufBinding);
        cb->draw(batch.count);
    }
}

} // namespace nx::pathkit
