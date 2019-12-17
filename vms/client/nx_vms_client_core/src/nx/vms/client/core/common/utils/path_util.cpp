#include "path_util.h"

#include <QtQml/QtQml>
#include <QtGui/QVector3D>

#include <nx/utils/log/assert.h>
#include <nx/client/core/utils/geometry.h>

namespace nx::vms::client::core {

namespace {

qreal normalAngle(const QPointF& point)
{
    return Geometry::atan2(point) - M_PI / 2;
}

qreal cornerNormalAngle(const QPointF& a, const QPointF& b, const QPointF& c)
{
    const auto angle1 = Geometry::atan2(a - b);
    auto angle2 = Geometry::atan2(c - b);
    if (angle2 < angle1)
        angle2 += M_PI * 2;
    return (angle1 + angle2) / 2;
}

} // namespace

PathUtil::PathUtil(QObject* parent):
    QObject(parent)
{
}

QVector<QPointF> PathUtil::points() const
{
    return m_points;
}

void PathUtil::setPoints(const QVector<QPointF>& points)
{
    m_points = points;
    updatePath();
}

QVariantList PathUtil::pointsVariantList() const
{
    QVariantList result;

    for (const auto& point: points())
        result.append(point);

    return result;
}

void PathUtil::setPointsVariantList(const QVariantList &points)
{
    QVector<QPointF> result;
    result.reserve(points.size());

    for (const QVariant& var: points)
    {
        if (var.canConvert(QVariant::PointF))
            result.append(var.toPointF());
    }

    setPoints(result);
}

bool PathUtil::isClosed() const
{
    return m_closed;
}

void PathUtil::setClosed(bool closed)
{
    if (m_closed == closed)
        return;

    m_closed = closed;
    emit pathChanged();
}

QRectF PathUtil::boundingRect() const
{
    return m_path.boundingRect();
}

bool PathUtil::contains(const QPointF& point) const
{
    return m_path.contains(point);
}

qreal PathUtil::length() const
{
    return m_length;
}

QPointF PathUtil::midAnchorPoint() const
{
    return m_midAnchorPoint;
}

qreal PathUtil::midAnchorPointNormalAngle() const
{
    return m_midAnchorPointNormalAngle;
}

bool PathUtil::checkSelfIntersections() const
{
    if (m_points.size() < 4)
        return false;

    constexpr auto crossProductZ =
        [](const QPointF& a, const QPointF& b)
        {
            return QVector3D::crossProduct(QVector3D(a), QVector3D(b)).z();
        };

    constexpr auto intersect =
        [crossProductZ](const QPointF& s1, const QPointF& e1, const QPointF& s2, const QPointF& e2)
        {
            return QRectF(s1, e1).intersects(QRectF(s2, e2))
                && crossProductZ(e1 - s1, s2 - s1) * crossProductZ(e1 - s1, e2 - s1) <= 0
                && crossProductZ(e2 - s2, s1 - s2) * crossProductZ(e2 - s2, e1 - s2) <= 0;
        };

    // Naive algorithm of self-intersection check. Try to find intersection of any pair of
    // segments.
    auto points = m_points;
    if (m_closed)
        points.append(m_points.first());

    for (int i = 3; i < points.size(); ++i)
    {
        for (int j = (m_closed && i == points.size() - 1) ? 2 : 1; j <= i - 2; ++j)
        {
            if (intersect(points[i - 1], points[i], points[j - 1], points[j]))
                return true;
        }
    }
    return false;
}

void PathUtil::registerQmlType()
{
    qmlRegisterType<PathUtil>("nx.vms.client.core", 1, 0, "PathUtil");
}

void PathUtil::updatePath()
{
    QPainterPath path;

    if (!m_points.isEmpty())
    {
        path.moveTo(m_points.first());
        for (auto it = std::next(m_points.begin()); it != m_points.end(); ++it)
            path.lineTo(*it);

        if (m_points.size() > 2)
            path.lineTo(m_points.first());

        path.setFillRule(Qt::WindingFill);
    }

    m_path = path;

    calculateMidAnchorPoint();

    emit pathChanged();
}

void PathUtil::calculateMidAnchorPoint()
{
    m_length = 0;
    m_midAnchorPoint = {};

    if (m_points.size() < 2)
        return;

    QVector<qreal> endPoints;
    endPoints.reserve(m_points.size() + 1);
    endPoints.append(0);

    QPointF prevPoint = m_points.first();
    for (auto it = std::next(m_points.begin()); it != m_points.end(); ++it)
    {
        m_length += Geometry::length(*it - prevPoint);
        endPoints.append(m_length);
        prevPoint = *it;
    }

    const qreal searchLength = m_length / 2;
    auto it = std::find_if(endPoints.begin(), endPoints.end(),
        [&](qreal p){ return p > searchLength; });

    if (it == endPoints.end())
        return;

    const auto index = (int) std::distance(endPoints.begin(), it);

    const qreal distanceToStart = std::abs(*(it - 1) - searchLength);
    const qreal distanceToEnd = std::abs(*it - searchLength);
    const qreal distanceToMid = std::abs((*it + *(it - 1)) / 2 - searchLength);

    if (distanceToMid < distanceToStart)
    {
        if (distanceToMid < distanceToEnd || index == m_points.size() - 1)
        {
            m_midAnchorPoint = (m_points[index - 1] + m_points[index]) / 2;
            m_midAnchorPointNormalAngle = normalAngle(m_points[index] - m_points[index - 1]);
        }
        else
        {
            m_midAnchorPoint = m_points[index];
            m_midAnchorPointNormalAngle =
                cornerNormalAngle(m_points[index - 1], m_points[index], m_points[index + 1]);
        }
    }
    else
    {
        if (index > 1)
        {
            m_midAnchorPoint = m_points[index - 1];
            m_midAnchorPointNormalAngle =
                cornerNormalAngle(m_points[index - 2], m_points[index - 1], m_points[index]);
        }
        else
        {
            m_midAnchorPoint = (m_points[index - 1] + m_points[index]) / 2;
            m_midAnchorPointNormalAngle = normalAngle(m_points[index] - m_points[index - 1]);
        }
    }
}

} // namespace nx::vms::client::core
