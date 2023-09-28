// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "figure.h"

#include <QtCore/QJsonObject>
#include <QtGui/QPolygonF>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/geometry.h>

namespace {

using nx::vms::client::core::Geometry;
using namespace nx::vms::client::desktop::figure;

QPointF toAbsolutePos(
    const QPointF& relativePoint,
    const QSizeF& boundSize)
{
    const QRectF widgetRect({}, boundSize);
    return Geometry::subPoint(widgetRect, relativePoint);
};

Points toAbsolutePos(
    const Points& relativePoints,
    const QSizeF& boundSize)
{
    nx::vms::client::desktop::figure::Points result;
    for (const auto& point: relativePoints)
        result.append(toAbsolutePos(point, boundSize));
    return result;
};

Points translatePoints(
    const Points& source,
    const Points::value_type& point)
{
    if (point.isNull())
        return source;

    Points result;
    for (const auto& sourcePoint: source)
        result.push_back(sourcePoint + point);
    return result;
}

} // namespace

namespace nx::vms::client::desktop::figure {

Figure::Figure(
    const FigureType type,
    const Points& points,
    const QColor& color,
    const QRectF& sceneRect,
    core::StandardRotation sceneRotation)
    :
    m_type(type),
    m_sourcePoints(points),
    m_color(color),
    m_sceneRotation(sceneRotation),
    m_sceneRect(sceneRect)
{
    updatePointValues();
}

Figure::~Figure()
{
}

FigureType Figure::type() const
{
    return m_type;
}

QColor Figure::color() const
{
    return m_color;
}

bool Figure::intersects(const QRectF& rect) const
{
    const auto figureBounds = m_targetBoundingRect.translated(m_targetPos);
    return figureBounds.isEmpty()
        ? rect.contains(figureBounds.topLeft())
        : figureBounds.intersects(rect);
}

QRectF Figure::sceneRect() const
{
    return m_sceneRect;
}

void Figure::setSceneRect(const QRectF& value)
{
    if (m_sceneRect == value)
        return;

    m_sceneRect = value;
    updatePointValues();
}

core::StandardRotation Figure::sceneRotation() const
{
    return m_sceneRotation;
}

void Figure::setSceneRotation(core::StandardRotation value)
{
    if (m_sceneRotation == value)
        return;

    m_sceneRotation = value;
    updatePointValues();
}

Points::value_type Figure::pos(const QSizeF& scale) const
{
    NX_ASSERT(scale.isValid(), "Scale should be valid");
    return toAbsolutePos(m_targetPos, scale);
}

Points Figure::points(const QSizeF& scale) const
{
    NX_ASSERT(scale.isValid(), "Scale should be valid");
    return toAbsolutePos(m_targetPoints, scale);
}

int Figure::pointsCount() const
{
    return m_sourcePoints.size();
}

QRectF Figure::boundingRect(const QSizeF& scale) const
{
    NX_ASSERT(scale.isValid(), "Scale should be valid");
    return Figure::visualRect(scale).translated(pos(scale));
}

bool Figure::containsPoint(
    const Points::value_type& /*point*/,
    const QSizeF& /*scale*/) const
{
    return false;
}

QRectF Figure::visualRect(const QSizeF& scale) const
{
    // Returns bounding rectangle by default.
    return QRectF(QPointF(), toAbsolutePos(m_targetBoundingRect.bottomRight(), scale));
}

bool Figure::isValid() const
{
    return m_type != FigureType::invalid && !m_targetPoints.isEmpty();
}

bool Figure::equalsTo(const Figure& right) const
{
    return type() == right.type()
        && sourcePoints() == right.sourcePoints()
        && color() == right.color();
}

void Figure::updatePointValues()
{
    Points updatedPoints = m_sourcePoints;

    // Moving points to the target scene rectangle coordinate system.
    if (m_sceneRect.isValid())
    {
        for (auto& point: updatedPoints)
            point = Geometry::cwiseDiv(point - m_sceneRect.topLeft(), m_sceneRect.size());
    }

    for (auto& point: updatedPoints)
        point = Geometry::rotateRelativePoint(point, m_sceneRotation);

    m_targetPos = QPolygonF(updatedPoints).boundingRect().topLeft();
    m_targetPoints = translatePoints(updatedPoints, -m_targetPos);
    m_targetBoundingRect = QPolygonF(updatedPoints).boundingRect().translated(-m_targetPos);
}

const Points& Figure::sourcePoints() const
{
    return m_sourcePoints;
}

bool operator==(const Figure& left, const Figure& right)
{
    return left.equalsTo(right);
}

bool operator!=(const Figure& left, const Figure& right)
{
    return !(left == right);
}

} // namespace nx::vms::client::desktop::figure
