// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "route.h"

bool QnOldRoute::isValid() const {
    return !points.isEmpty();
}

bool QnOldRoute::isEqual(const QnOldRoute &other) const {
    return points == other.points;
}

int QnOldRoute::length() const {
    return points.size();
}

bool QnOldRoute::addPoint(const QnRoutePoint &point, int weight) {
    // prevent loops in routes
    for (const QnRoutePoint &p: points) {
        if (p.peerId == point.peerId)
            return false;
    }

    points.append(point);
    this->weight += weight;
    return true;
}

bool QnOldRoute::containsConnection(const nx::Uuid &first, const nx::Uuid &from, const QnRoutePoint &point) const {
    nx::Uuid prevId = first;
    for (auto it = points.begin(); it != points.end(); ++it) {
        if (prevId == from)
            return *it == point;
        prevId = it->peerId;
    }
    return false;
}

bool QnOldRoute::containsPoint(const nx::Uuid &id) const {
    for (const QnRoutePoint &point: points) {
        if (point.peerId == id)
            return true;
    }
    return false;
}

bool QnOldRoute::operator <(const QnOldRoute &other) const {
    if (weight != other.weight)
        return weight < other.weight;
    else
        return points.size() < other.points.size();
}

QnOldRoute QnOldRoute::operator +(const QnOldRoute &other) const {
    QnOldRoute route = *this;
    route.points.append(other.points);
    route.weight += other.weight;
    return route;
}

bool QnRoutePoint::operator ==(const QnRoutePoint &other) const {
    return peerId == other.peerId && host == other.host && port == other.port;
}
