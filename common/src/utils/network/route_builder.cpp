#include "route_builder.h"

#include <QtCore/QQueue>

QnRouteBuilder::QnRouteBuilder(const QnUuid &startId) :
    m_startId(startId)
{
}

void QnRouteBuilder::addConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port, int weight) {
    Q_ASSERT(from != to);

    if (to == m_startId)
        return;

    QnRoutePoint point(to, host, port);

    if (findConnection(from, point) != m_connections.end())
        return;

    m_connections.insert(from, WeightedPoint(QnRoutePoint(to, host, port), weight));
}

void QnRouteBuilder::removeConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port) {
    Q_ASSERT(from != to);

    QnRoutePoint point(to, host, port);

    auto it = findConnection(from, point);
    if (it == m_connections.end())
        return;
    m_connections.erase(it);

    for (auto it = m_routes.begin(); it != m_routes.end(); /* no inc */) {
        if (it->containsConnection(m_startId, from, point))
            it = m_routes.erase(it);
        else
            ++it;
    }
}

void QnRouteBuilder::clear() {
    m_routes.clear();
}

QnRoute QnRouteBuilder::routeTo(const QnUuid &peerId) {
    QnRoute route = m_routes.value(peerId);
    if (route.isValid())
        return route;

    route = buildRouteTo(peerId);
    m_routes[peerId] = route;
    return route;
}

QHash<QnUuid, QnRoute> QnRouteBuilder::routes() const {
    return m_routes;
}

QnRoute QnRouteBuilder::buildRouteTo(const QnUuid &peerId) {
    QHash<QnUuid, QnUuid> trace;
    QHash<QPair<QnUuid, QnUuid>, WeightedPoint> usedPoints;
    QQueue<QnUuid> points;
    QSet<QnUuid> checkedPoints;

    points.enqueue(m_startId);
    checkedPoints.insert(m_startId);

    while (!points.isEmpty()) {
        QnUuid current = points.dequeue();

        for (const WeightedPoint &point: m_connections.values(current)) {
            QnUuid id = point.first.peerId;
            if (checkedPoints.contains(id))
                continue;

            trace[id] = current;
            usedPoints.insert(QPair<QnUuid, QnUuid>(current, id), point);

            if (id == peerId) {
                QList<WeightedPoint> points;
                QnUuid id = peerId;
                while (id != m_startId) {
                    QnUuid source = trace.value(id);
                    points.prepend(usedPoints.value(QPair<QnUuid, QnUuid>(source, id)));
                    id = source;
                }

                QnRoute route;
                for (const WeightedPoint &point: points)
                    route.addPoint(point.first, point.second);

                return route;
            }

            checkedPoints.insert(id);
            points.enqueue(id);
        }
    }

    return QnRoute();
}

QMultiHash<QnUuid, QnRouteBuilder::WeightedPoint>::iterator QnRouteBuilder::findConnection(const QnUuid &from, const QnRoutePoint &point) {
    auto it = m_connections.begin();
    for (; it != m_connections.end(); ++it) {
        if (it.key() == from && it.value().first == point)
            break;
    }
    return it;
}
