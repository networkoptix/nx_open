// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPaintEngine>

#include "include/core/SkPath.h"

class QRhi;
class QRhiResourceUpdateBatch;
class QRhiRenderPassDescriptor;
class QRhiCommandBuffer;

namespace nx::pathkit {

class RhiPaintDevice;

struct NX_PATHKIT_API PaintPath
{
    QPen pen;
    QBrush brush;
    pk::SkPath path;
    bool aa = false;
    qreal opacity = 1.0;
    std::optional<pk::SkPath> clip;

    ~PaintPath();
};

struct PaintPixmap
{
    QRectF dst;
    QPixmap pixmap;
    QRectF src;
    QTransform transform;
    qreal opacity = 1.0;
};

struct PaintCustom
{
    std::function<void(
        QRhi*,
        QRhiRenderPassDescriptor*,
        QRhiResourceUpdateBatch*,
        const QMatrix4x4& deviceMvp)> prepare;

    std::function<void(QRhi*, QRhiCommandBuffer*, QSize)> render;
};

using PaintData = std::variant<PaintPath, PaintPixmap, PaintCustom>;

/**
 * A paint engine implementation which generates a list of drawing paths using Skia PathOps API.
 */
class NX_PATHKIT_API RhiPaintEngine: public QPaintEngine
{
    using base_type = QPaintEngine;

    friend class RhiPaintDevice;
    friend class RhiPaintDeviceRenderer;

public:
    RhiPaintEngine(RhiPaintDevice* device);

    bool begin(QPaintDevice *pdev) override;
    bool end() override;

    void updateState(const QPaintEngineState& state) override;
    Type type() const override;

    void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) override;
    void drawPath(const QPainterPath& path) override;

    void drawPolygon(const QPointF* points, int pointCount, QPaintEngine::PolygonDrawMode mode) override;

    void drawCustom(const PaintCustom& custom);

private:
    void updateClipPath(const QPainterPath& clipPath, Qt::ClipOperation op);
    std::optional<pk::SkPath> getClip() const;

    std::vector<PaintData> m_paths;
    RhiPaintDevice* m_device = nullptr;

    bool m_clipEnabled = false;
    pk::SkPath m_clipPath;
};

} // namespace nx::pathkit
