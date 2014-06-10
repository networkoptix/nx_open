#include "route_builder.h"

/* QnRouteBuilder::Route */

bool QnRouteBuilder::Route::isValid() const {
    return !points.isEmpty();
}

bool QnRouteBuilder::Route::addPoint(const QnRouteBuilder::RoutePoint &point, int weight) {
    // prevent loops in routes
    foreach (const RoutePoint &p, points) {
        if (p.peerId == point.peerId)
            return false;
    }

    points.append(point);
    this->weight += weight;
    return true;
}

bool QnRouteBuilder::Route::containsConnection(const QnId &from, const QnRouteBuilder::RoutePoint &point) const {
    QnId prevId = points.first().peerId;
    for (auto it = points.begin() + 1; it != points.end(); ++it) {
        if (prevId == from)
            return *it == point;
        prevId = it->peerId;
    }
    return false;
}

bool QnRouteBuilder::Route::operator <(const QnRouteBuilder::Route &other) const {
    if (weight != other.weight)
        return weight < other.weight;
    else
        return points.size() < other.points.size();
}

/* QnRouteBuilder::RoutePoint */

bool QnRouteBuilder::RoutePoint::operator ==(const QnRouteBuilder::RoutePoint &other) const {
    return peerId == other.peerId && host == other.host && port == other.port;
}

/* QnRouteBuilder */

QnRouteBuilder::QnRouteBuilder(const QnId &startId) :
    m_startId(startId)
{
}

void QnRouteBuilder::addConnection(const QnId &from, const QnId &to, const QString &host, quint16 port, int weight) {
    Q_ASSERT(from != to);

    RoutePoint point(to, host, port);

    if (findConnection(from, point) != m_connections.end())
        return;

    m_connections.insert(from, WeightedPoint(RoutePoint(to, host, port), weight));

    if (m_startId == from) {
        // we've got a new direct connection
        Route route;
        route.addPoint(point, weight);
        buildRoutes(QList<Route>() << route);
    } else {
        // we've got a new point, so we have to update routes
        QList<Route> routes;
        foreach (const Route &route, m_routes[from]) {
            Route newRoute = route;
            newRoute.addPoint(point, weight);
            routes.append(newRoute);
        }
        buildRoutes(routes);
    }
}

void QnRouteBuilder::removeConnection(const QnId &from, const QnId &to, const QString &host, quint16 port) {
    Q_ASSERT(from != to);

    RoutePoint point(to, host, port);

    auto it = findConnection(from, RoutePoint(to, host, port));
    if (it == m_connections.end())
        return;
    m_connections.erase(it);

    if (m_startId == from) {
        // remove the direct connection
        RouteList &routeList = m_routes[to];
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

QnRouteBuilder::Route QnRouteBuilder::routeTo(const QnId &peerId) const {
    const RouteList &routeList = m_routes[peerId];
    if (routeList.isEmpty())
        return Route();
    else
        return routeList.first();
}

void QnRouteBuilder::insertRoute(const QnRouteBuilder::Route &route) {
    Q_ASSERT(!route.points.isEmpty());

    RouteList &routeList = m_routes[route.points.last().peerId];
    routeList.insert(qLowerBound(routeList.begin(), routeList.end(), route), route);
}

void QnRouteBuilder::buildRoutes(const QList<QnRouteBuilder::Route> &initialRoutes) {
    QList<QnRouteBuilder::Route> routes = initialRoutes;

    while (!routes.isEmpty()) {
        Route route = routes.takeFirst();

        foreach (const WeightedPoint &point, m_connections.values(route.points.last().peerId)) {
            Route newRoute = route;
            newRoute.addPoint(point.first, point.second);
            routes.append(route);
        }

        insertRoute(route);
    }
}

QMultiHash<QnId, QnRouteBuilder::WeightedPoint>::iterator QnRouteBuilder::findConnection(const QnId &from, const QnRouteBuilder::RoutePoint &point) {
    auto it = m_connections.begin();
    for (; it != m_connections.end(); ++it) {
        if (it.key() == from && it.value().first == point)
            break;
    }
    return it;
}
