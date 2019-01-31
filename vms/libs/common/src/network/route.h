#ifndef ROUTE_H
#define ROUTE_H

#include <utils/common/id.h>

struct QnRoutePoint {
    QnUuid peerId;
    QString host;
    quint16 port;

    QnRoutePoint() {}

    QnRoutePoint(const QnUuid &peerId, const QString &host, quint16 port) :
        peerId(peerId), host(host), port(port)
    {}

    bool operator ==(const QnRoutePoint &other) const;
};

struct QnOldRoute {
    QList<QnRoutePoint> points;
    int weight;

    QnOldRoute() : weight(0) {}

    bool isValid() const;
    bool isEqual(const QnOldRoute &other) const;

    int length() const;

    bool addPoint(const QnRoutePoint &point, int weight);
    bool containsConnection(const QnUuid &first, const QnUuid &from, const QnRoutePoint &point) const;
    bool containsPoint(const QnUuid &id) const;

    bool operator <(const QnOldRoute &other) const;
    QnOldRoute operator +(const QnOldRoute &other) const;
};

typedef QList<QnOldRoute> QnRouteList;

#endif // ROUTE_H
