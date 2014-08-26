#include "route_builder.h"

QnRouteBuilder::QnRouteBuilder(const QUuid &startId) :
    m_startId(startId)
{
}

void QnRouteBuilder::addConnection(const QUuid &from, const QUuid &to, const QString &host, quint16 port, int weight) {
    Q_ASSERT(from != to);

    if (to == m_startId)
        return;

    QnRoutePoint point(to, host, port);

    if (findConnection(from, point) != m_connections.end())
        return;

    m_connections.insert(from, WeightedPoint(QnRoutePoint(to, host, port), weight));

    if (m_startId == from) {
        // we've got a new direct connection
        QnRoute route;
        route.addPoint(point, weight);
        buildRoutes(QList<QnRoute>() << route);
    } else {
        // we've got a new point, so we have to update routes
        QList<QnRoute> routes;
        foreach (const QnRoute &route, m_routes[from]) {
            QnRoute newRoute = route;
            newRoute.addPoint(point, weight);
            routes.append(newRoute);
        }
        buildRoutes(routes);
    }
}

void QnRouteBuilder::removeConnection(const QUuid &from, const QUuid &to, const QString &host, quint16 port) {
    Q_ASSERT(from != to);

    QnRoutePoint point(to, host, port);

    auto it = findConnection(from, QnRoutePoint(to, host, port));
    if (it == m_connections.end())
        return;
    m_connections.erase(it);

    if (m_startId == from) {
        // remove the direct connection
        QnRouteList &routeList = m_routes[to];
        for (auto it = routeList.begin(); it != routeList.end(); ++it) {
            if (it->points.size() == 1 && it->points.first() == point) {
                routeList.erase(it);
                break;
            }
        }
    } else {
        // remove all routes containing the given connection
        for (auto rtIt = m_routes.begin(); rtIt != m_routes.end(); ++rtIt) {
            for (auto it = rtIt.value().begin(); it != rtIt.value().end(); ) {
                if (it->containsConnection(from, point))
                    it = rtIt.value().erase(it);
                else
                    ++it;
            }
        }
    }
}

void QnRouteBuilder::clear() {
    m_routes.clear();
}

QnRoute QnRouteBuilder::routeTo(const QUuid &peerId) const {
    const QnRouteList &routeList = m_routes[peerId];
    if (routeList.isEmpty())
        return QnRoute();
    else
        return routeList.first();
}

QHash<QUuid, QnRouteList> QnRouteBuilder::routes() const {
    return m_routes;
}

void QnRouteBuilder::insertRoute(const QnRoute &route) {
    Q_ASSERT(!route.points.isEmpty());

    QnRouteList &routeList = m_routes[route.points.last().peerId];
    routeList.insert(qLowerBound(routeList.begin(), routeList.end(), route), route);
}

void QnRouteBuilder::buildRoutes(const QList<QnRoute> &initialRoutes) {
    QList<QnRoute> routes = initialRoutes;

    while (!routes.isEmpty()) {
        QnRoute route = routes.takeFirst();

        foreach (const WeightedPoint &point, m_connections.values(route.points.last().peerId)) {
            QnRoute newRoute = route;
            if (newRoute.addPoint(point.first, point.second))
                routes.append(newRoute);
        }

        insertRoute(route);
    }
}

QMultiHash<QUuid, QnRouteBuilder::WeightedPoint>::iterator QnRouteBuilder::findConnection(const QUuid &from, const QnRoutePoint &point) {
    auto it = m_connections.begin();
    for (; it != m_connections.end(); ++it) {
        if (it.key() == from && it.value().first == point)
            break;
    }
    return it;
}
