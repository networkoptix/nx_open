#include "path_util.h"

#include <QtQml/QtQml>
#include <QtGui/QVector3D>

namespace nx::vms::client::core {

PathUtil::PathUtil(QObject* parent):
    QObject(parent)
{
}

QVariantList PathUtil::points()
{
    QVariantList result;

    for (const auto& point: m_points)
        result.append(point);

    return result;
}

void PathUtil::setPoints(const QVariantList& points)
{
    m_points.clear();

    for (const QVariant& var: points)
    {
        if (var.canConvert(QVariant::PointF))
            m_points.append(var.toPointF());
    }

    updatePath();
}

QRectF PathUtil::boundingRect() const
{
    return m_path.boundingRect();
}

bool PathUtil::contains(const QPointF& point) const
{
    return m_path.contains(point);
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
    points.append(m_points.first());

    for (int i = 3; i < points.size(); ++i)
    {
        for (int j = (i == points.size() - 1 ? 2 : 1); j <= i - 2; ++j)
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

    emit pathChanged();
}

} // namespace nx::vms::client::core
