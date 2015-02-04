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

    for (auto it = m_routes.begin(); it != m_routes.end(); /* no inc */) {
        if (it->containsPoint(from) || it->containsPoint(to))
            it = m_routes.erase(it);
        else
            ++it;
    }
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

QnRoute QnRouteBuilder::routeTo(const QnUuid &peerId, const QnUuid &via) {
	if (!via.isNull() && via != peerId && via != m_startId) {
		QnRoute preRoute = buildRouteTo(via);
		if (!preRoute.isValid())
			return QnRoute();

		QnRoute postRoute = buildRouteTo(peerId, via);
		if (!postRoute.isValid())
			return QnRoute();

		return preRoute + postRoute;
	}

    QnRoute route = m_routes.value(peerId);
    if (route.isValid())
        return route;

    route = buildRouteTo(peerId);
    if (route.isValid())
        m_routes[peerId] = route;

    return route;
}

QHash<QnUuid, QnRoute> QnRouteBuilder::routes() const {
    return m_routes;
}

QnRoutePoint QnRouteBuilder::enforcedConnection() const {
    return m_enforcedConnection;
}

void QnRouteBuilder::setEnforcedConnection(const QnRoutePoint &enforcedConnection) {
    m_enforcedConnection = enforcedConnection;
}

QnRoute QnRouteBuilder::buildRouteTo(const QnUuid &peerId, const QnUuid &from) {
    QHash<QnUuid, QnUuid> trace;
    QHash<QPair<QnUuid, QnUuid>, WeightedPoint> usedPoints;
    QQueue<QnUuid> points;
    QSet<QnUuid> checkedPoints;
    QnUuid startId = from.isNull() ? m_startId : from;

    points.enqueue(startId);
    checkedPoints.insert(startId);

    while (!points.isEmpty()) {
        QnUuid current = points.dequeue();

        QList<WeightedPoint> connections = m_connections.values(current);
        if (current == m_startId && !m_enforcedConnection.peerId.isNull()) {
            bool contains = false;
            for (const WeightedPoint &point: connections) {
                if (point.first == m_enforcedConnection) {
                    contains = true;
                    break;
                }
            }
            if (!contains)
                connections.append(WeightedPoint(m_enforcedConnection, DEFAULT_ROUTE_POINT_WEIGHT));
        }

        for (const WeightedPoint &point: connections) {
            QnUuid id = point.first.peerId;
            if (checkedPoints.contains(id))
                continue;

            trace[id] = current;
            usedPoints.insert(QPair<QnUuid, QnUuid>(current, id), point);

            if (id == peerId) {
                QList<WeightedPoint> points;
                QnUuid id = peerId;
                while (id != startId) {
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
