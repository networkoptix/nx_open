#include "path_util.h"

#include <QtQml/QtQml>

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
