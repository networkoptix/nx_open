#include "route_builder.h"

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

    QList<QnRoute> routes;
    if (m_startId == from) {
        // we've got a new direct connection
        QnRoute newRoute;
        newRoute.addPoint(point, weight);
        routes.append(newRoute);
    } else {
        // we've got a new point, so we have to update routes
        foreach (const QnRoute &route, m_routes.value(from)) {
            QnRoute newRoute = route;
            newRoute.addPoint(point, weight);
            routes.append(newRoute);
        }
    }
    buildRoutes(routes);
}

void QnRouteBuilder::removeConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port) {
    Q_ASSERT(from != to);

    QnRoutePoint point(to, host, port);

    auto it = findConnection(from, QnRoutePoint(to, host, port));
    if (it == m_connections.end())
        return;
    m_connections.erase(it);

    // remove all routes containing the given connection
    for (auto rtIt = m_routes.begin(); rtIt != m_routes.end(); ++rtIt) {
        for (auto it = rtIt.value().begin(); it != rtIt.value().end(); ) {
            if (it->containsConnection(m_startId, from, point))
                it = rtIt.value().erase(it);
            else
                ++it;
        }
    }
}

void QnRouteBuilder::clear() {
    m_routes.clear();
}

QnRoute QnRouteBuilder::routeTo(const QnUuid &peerId) const {
    const QnRouteList &routeList = m_routes[peerId];
    if (routeList.isEmpty())
        return QnRoute();
    else
        return routeList.first();
}

QHash<QnUuid, QnRouteList> QnRouteBuilder::routes() const {
    return m_routes;
}

bool QnRouteBuilder::insertRoute(const QnRoute &route) {
    Q_ASSERT(!route.points.isEmpty());

    QnRouteList &routeList = m_routes[route.points.last().peerId];

    foreach (const QnRoute &existentRoute, routeList) {
        if (existentRoute.isEqual(route))
            return false;
    }

    routeList.insert(qLowerBound(routeList.begin(), routeList.end(), route), route);
    return true;
}

void QnRouteBuilder::buildRoutes(const QList<QnRoute> &initialRoutes) {
    QList<QnRoute> routes = initialRoutes;

    while (!routes.isEmpty()) {
        QnRoute route = routes.takeFirst();

        if (!insertRoute(route))
            continue;

        foreach (const WeightedPoint &point, m_connections.values(route.points.last().peerId)) {
            QnRoute newRoute = route;
            if (newRoute.addPoint(point.first, point.second))
                routes.append(newRoute);
        }
    }
}

QMultiHash<QnUuid, QnRouteBuilder::WeightedPoint>::iterator QnRouteBuilder::findConnection(const QnUuid &from, const QnRoutePoint &point) {
    auto it = m_connections.begin();
    for (; it != m_connections.end(); ++it) {
        if (it.key() == from && it.value().first == point)
            break;
    }
    return it;
}
