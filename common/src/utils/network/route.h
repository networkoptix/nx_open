#ifndef ROUTE_H
#define ROUTE_H

#include <utils/common/id.h>

struct QnRoutePoint {
    QnId peerId;
    QString host;
    quint16 port;

    QnRoutePoint(const QnId &peerId, const QString &host, quint16 port) :
        peerId(peerId), host(host), port(port)
    {}

    bool operator ==(const QnRoutePoint &other) const;
};

struct QnRoute {
    QList<QnRoutePoint> points;
    int weight;

    bool isValid() const;

    bool addPoint(const QnRoutePoint &point, int weight);
    bool containsConnection(const QnId &from, const QnRoutePoint &point) const;

    bool operator <(const QnRoute &other) const;
};

#endif // ROUTE_H
