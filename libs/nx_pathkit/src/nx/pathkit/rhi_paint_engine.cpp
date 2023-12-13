// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_paint_engine.h"

#include <QtGui/QVector2D>
#include <QtGui/QPainterPath>

#include <include/core/SkPath.h>
#include <include/pathops/SkPathOps.h>

using namespace pk;

namespace {

static void applyPolygonModePath(SkPath& skPath, QPaintEngine::PolygonDrawMode mode)
{
    switch (mode)
    {
        case QPaintEngine::OddEvenMode:
            skPath.setFillType(SkPathFillType::kEvenOdd);
            break;
        case QPaintEngine::WindingMode:
            skPath.setFillType(SkPathFillType::kWinding);
            break;

        // Handled in skPath.addPoly().
        case QPaintEngine::ConvexMode:
        case QPaintEngine::PolylineMode:
        default:
            break;
    }
}

SkPathFillType fillRuleToSk(Qt::FillRule fillRule)
{
    switch (fillRule)
    {
        case Qt::WindingFill:
            return SkPathFillType::kWinding;
        case Qt::OddEvenFill:
        default:
            return SkPathFillType::kEvenOdd;
    }
}

QPen scalePen(const QPen& pen, QTransform tr)
{
    auto newPen = pen;

    if (newPen.width() == 0)
    {
        newPen.setWidth(1);
    }
    else if (!newPen.isCosmetic())
    {
        const auto p1 = tr.map(QPointF(0, 0));
        const auto p2 = tr.map(QPointF(newPen.widthF(), newPen.widthF()));
        newPen.setWidthF(QVector2D(p2 - p1).length() / std::sqrt(2));
    }

    return newPen;
}

SkPath painterPathToSkPath(const QPainterPath& path, const QTransform& tr)
{
    SkPath skPath;

    for (int i = 0; i < path.elementCount(); ++i)
    {
        const QPainterPath::Element e = path.elementAt(i);

        const auto p0 = tr.map(QPointF{e.x, e.y});

        if (e.type == QPainterPath::MoveToElement)
        {
            skPath.moveTo(p0.x(), p0.y());
        }
        else if (e.type == QPainterPath::LineToElement)
        {
            skPath.lineTo(p0.x(), p0.y());
        }
        else if (e.type == QPainterPath::CurveToElement)
        {
            const bool isCubic = i + 2 < path.elementCount()
                && path.elementAt(i + 2).type == QPainterPath::CurveToDataElement;

            const auto e1 = path.elementAt(i + 1);
            const auto p1 = tr.map(QPointF{e1.x, e1.y});

            if (isCubic)
            {
                const auto e2 = path.elementAt(i + 2);
                const auto p2 = tr.map(QPointF{e2.x, e2.y});

                skPath.cubicTo(p0.x(), p0.y(), p1.x(), p1.y(), p2.x(), p2.y());
                i += 2;
            }
            else
            {
                skPath.quadTo(p0.x(), p0.y(), p1.x(), p1.y());
                i += 1;
            }
        }
    }

    skPath.setFillType(fillRuleToSk(path.fillRule()));

    return skPath;
}

} // namespace

namespace nx::pathkit {

PaintPath::~PaintPath() {}

RhiPaintEngine::RhiPaintEngine(RhiPaintDevice* device):
    base_type(
        QPaintEngine::Antialiasing
        | QPaintEngine::PaintOutsidePaintEvent
        | QPaintEngine::ConstantOpacity
        | QPaintEngine::PrimitiveTransform
        | QPaintEngine::PainterPaths
        | QPaintEngine::AlphaBlend
        | QPaintEngine::PixmapTransform
    ),
    m_device(device)
{
}

bool RhiPaintEngine::begin(QPaintDevice*)
{
    return true;
}

bool RhiPaintEngine::end()
{
    return true;
}

void RhiPaintEngine::updateState(const QPaintEngineState& state)
{
    const auto flags = state.state();

    if (flags & DirtyClipPath)
    {
        m_clipEnabled = true;
        updateClipPath(state.clipPath(), state.clipOperation());
    }
    else if (flags & DirtyClipRegion)
    {
        m_clipEnabled = true;
        QPainterPath path;
        for (const QRect& rect: state.clipRegion())
            path.addRect(rect);
        updateClipPath(path, state.clipOperation());
    }
    else if (flags & DirtyClipEnabled)
    {
        m_clipEnabled = state.isClipEnabled();
    }
}

QPaintEngine::Type RhiPaintEngine::type() const
{
    return static_cast<QPaintEngine::Type>(QPaintEngine::User + 1);
}

void RhiPaintEngine::drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr)
{
    m_paths.push_back(PaintPixmap{
        .dst = r,
        .pixmap = pm,
        .src = sr,
        .transform = state->transform(),
        .opacity = painter()->opacity()
    });
}

void RhiPaintEngine::drawPath(const QPainterPath& path)
{
    m_paths.push_back(PaintPath{
        .pen = scalePen(state->pen(), state->transform()),
        .brush = state->brush(),
        .path = painterPathToSkPath(path, state->transform()),
        .aa = state->renderHints().testFlag(QPainter::Antialiasing),
        .opacity = state->opacity(),
        .clip = getClip()
    });
}

std::optional<pk::SkPath> RhiPaintEngine::getClip() const
{
    if (m_clipEnabled)
        return m_clipPath;

    return {};
}

void RhiPaintEngine::drawPolygon(
    const QPointF* points,
    int pointCount,
    QPaintEngine::PolygonDrawMode mode)
{
    SkPath skPath;
    std::vector<SkPoint> skPoints;

    const auto transform = state->transform();

    for (int i = 0; i < pointCount; ++i)
    {
        const auto p = transform.map(points[i]);

        skPoints.emplace_back(SkPoint::Make(p.x(), p.y()));
    }

    skPath.addPoly(skPoints.data(), skPoints.size(), /*close*/ mode != QPaintEngine::PolylineMode);

    applyPolygonModePath(skPath, mode);

    m_paths.push_back(PaintPath{
        .pen = scalePen(state->pen(), state->transform()),
        .brush = state->brush(),
        .path = skPath,
        .aa = state->renderHints().testFlag(QPainter::Antialiasing),
        .opacity = state->opacity(),
        .clip = getClip()
    });
}

void RhiPaintEngine::drawCustom(const PaintCustom& custom)
{
    m_paths.push_back(custom);
}

void RhiPaintEngine::updateClipPath(const QPainterPath& clipPath, Qt::ClipOperation op)
{
    const SkPath skPath = painterPathToSkPath(clipPath, state->transform());

    switch (op)
    {
        case Qt::NoClip:
            m_clipEnabled = false;
            m_clipPath = {};
            break;

        case Qt::ReplaceClip:
            m_clipPath = skPath;
            break;

        case Qt::IntersectClip:
            Op(m_clipPath, skPath, SkPathOp::kIntersect_SkPathOp, &m_clipPath);
            break;
    }
}

} // namespace nx::pathkit
