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

struct QnRoute {
    QList<QnRoutePoint> points;
    int weight;

    QnRoute() : weight(0) {}

    bool isValid() const;
    bool isEqual(const QnRoute &other) const;

    int length() const;

    bool addPoint(const QnRoutePoint &point, int weight);
    bool containsConnection(const QnUuid &first, const QnUuid &from, const QnRoutePoint &point) const;

    bool operator <(const QnRoute &other) const;
	QnRoute operator +(const QnRoute &other) const;
};

typedef QList<QnRoute> QnRouteList;

#endif // ROUTE_H
