#include "route.h"

bool QnRoute::isValid() const {
    return !points.isEmpty();
}

bool QnRoute::isEqual(const QnRoute &other) const {
    return points == other.points;
}

bool QnRoute::addPoint(const QnRoutePoint &point, int weight) {
    // prevent loops in routes
    foreach (const QnRoutePoint &p, points) {
        if (p.peerId == point.peerId)
            return false;
    }

    points.append(point);
    this->weight += weight;
    return true;
}

bool QnRoute::containsConnection(const QUuid &from, const QnRoutePoint &point) const {
    QUuid prevId = points.first().peerId;
    for (auto it = points.begin() + 1; it != points.end(); ++it) {
        if (prevId == from)
            return *it == point;
        prevId = it->peerId;
    }
    return false;
}

bool QnRoute::operator <(const QnRoute &other) const {
    if (weight != other.weight)
        return weight < other.weight;
    else
        return points.size() < other.points.size();
}

bool QnRoutePoint::operator ==(const QnRoutePoint &other) const {
    return peerId == other.peerId && host == other.host && port == other.port;
}
