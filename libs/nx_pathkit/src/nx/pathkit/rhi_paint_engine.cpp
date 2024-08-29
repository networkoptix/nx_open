// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_paint_engine.h"

#include <QtGui/QVector2D>
#include <QtGui/QPainterPath>

#include <include/core/SkPath.h>
#include <include/pathops/SkPathOps.h>

#include <QtGui/rhi/qrhi.h>

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

SkPath painterPathToSkPath(
    const QPainterPath& path,
    const QTransform& tr,
    SkPathRefAllocator* allocator = nullptr)
{
    SkPath skPath(allocator);

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

SkMatrix transformToSk(const QTransform& t)
{
    if (t.isIdentity())
        return {};

    return SkMatrix::MakeAll(
        t.m11(), t.m21(), t.m31(),
        t.m12(), t.m22(), t.m32(),
        t.m13(), t.m23(), t.m33());
}

} // namespace

namespace nx::pathkit {

RhiPaintEngineSyncData::RhiPaintEngineSyncData()
{
    m_allocator.reset(SkPath::CreateAllocator());
}

void RhiPaintEngineSyncData::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
    m_allocator->clear();
    m_rendered = false;
}

void RhiPaintEngineSyncData::append(PaintData&& p)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rendered = false;
    m_entries.append(std::move(p));
}

void RhiPaintEngineSyncData::append(std::function<PaintPath(SkPathRefAllocator*)> locked)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rendered = false;
    m_entries.append(locked(m_allocator.get()));
}

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
    m_data = std::make_shared<RhiPaintEngineSyncData>();
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
        SkPath path;
        for (const QRect& r: state.clipRegion())
            path.addRect(r.left(), r.top(), r.right() + 1, r.bottom() + 1);
        path.transform(transformToSk(state.transform()));
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
    m_data->append(PaintPixmap{
        .dst = r,
        .pixmap = pm,
        .src = sr,
        .transform = state->transform(),
        .clip = getClip(),
        .opacity = painter()->opacity()
    });
}

void RhiPaintEngine::drawPath(const QPainterPath& path)
{
    m_data->append(
        [&](SkPathRefAllocator* allocator) -> PaintPath
        {
            return PaintPath{
                .pen = scalePen(state->pen(), state->transform()),
                .brush = state->brush(),
                .path = painterPathToSkPath(path, state->transform(), allocator),
                .aa = state->renderHints().testFlag(QPainter::Antialiasing),
                .opacity = state->opacity(),
                .clip = getClip()
            };
        });
}

std::optional<SkPath> RhiPaintEngine::getClip() const
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
    m_data->append(
        [&](SkPathRefAllocator* allocator) -> PaintPath
        {
            SkPath skPath(allocator);

            const auto transform = state->transform();

            if (pointCount > 0)
            {
                const auto p0 = transform.map(points[0]);
                skPath.moveTo(p0.x(), p0.y());

                for (int i = 1; i < pointCount; ++i)
                {
                    const auto p = transform.map(points[i]);
                    skPath.lineTo(p.x(), p.y());
                }

                if (mode != QPaintEngine::PolylineMode)
                    skPath.close();
            }

            applyPolygonModePath(skPath, mode);

            return PaintPath{
                .pen = scalePen(state->pen(), state->transform()),
                .brush = state->brush(),
                .path = skPath,
                .aa = state->renderHints().testFlag(QPainter::Antialiasing),
                .opacity = state->opacity(),
                .clip = getClip()
            };
        });
}

void RhiPaintEngine::drawCustom(const PaintCustom& custom)
{
    m_data->append(custom);
}

void RhiPaintEngine::drawTexture(
    const QRectF& dst, std::shared_ptr<QRhiTexture> texture, bool mirrorY)
{
    const auto size = texture->pixelSize();
    m_data->append(PaintTexture{
        .dst = dst,
        .texture = texture,
        .src = QRectF(0, 0, size.width(), size.height()), //< Currently unsused.
        .transform = state->transform(),
        .opacity = painter()->opacity(),
        .mirrorY = mirrorY
    });
}

void RhiPaintEngine::updateClipPath(const QPainterPath& clipPath, Qt::ClipOperation op)
{
    if (op == Qt::NoClip)
    {
        updateClipPath(SkPath{}, op);
        return;
    }

    const SkPath skPath = painterPathToSkPath(clipPath, state->transform());
    updateClipPath(skPath, op);
}

void RhiPaintEngine::updateClipPath(const SkPath& skPath, Qt::ClipOperation op)
{
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
