#ifndef ROUTE_H
#define ROUTE_H

#include <utils/common/id.h>

struct QnRoutePoint {
    QUuid peerId;
    QString host;
    quint16 port;

    QnRoutePoint(const QUuid &peerId, const QString &host, quint16 port) :
        peerId(peerId), host(host), port(port)
    {}

    bool operator ==(const QnRoutePoint &other) const;
};

struct QnRoute {
    QList<QnRoutePoint> points;
    int weight;

    QnRoute() : weight(0) {}

    bool isValid() const;
    bool isEqual(const QnRoute &other) const;

    bool addPoint(const QnRoutePoint &point, int weight);
    bool containsConnection(const QUuid &first, const QUuid &from, const QnRoutePoint &point) const;

    bool operator <(const QnRoute &other) const;
};

typedef QList<QnRoute> QnRouteList;

#endif // ROUTE_H
