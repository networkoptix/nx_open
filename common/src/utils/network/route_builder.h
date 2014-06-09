#ifndef ROUTE_BUILDER_H
#define ROUTE_BUILDER_H

#include <QtCore/QString>

#include <utils/common/id.h>

class QnRouteBuilder {
public:

    struct RoutePoint {
        QnId peerId;
        QString host;
        quint16 port;

        RoutePoint(const QnId &peerId, const QString &host, quint16 port) :
            peerId(peerId), host(host), port(port)
        {}

        bool operator ==(const RoutePoint &other) const;
    };

    struct Route {
        QList<RoutePoint> points;
        int weight;

        bool isValid() const;

        bool addPoint(const RoutePoint &point, int weight);
        bool containsConnection(const QnId &from, const RoutePoint &point) const;

        bool operator <(const Route &other) const;
    };

    explicit QnRouteBuilder(const QnId &startId);

    void addConnection(const QnId &from, const QnId &to, const QString &host, quint16 port, int weight = 5);
    void removeConnection(const QnId &from, const QnId &to, const QString &host, quint16 port);

    Route routeTo(const QnId &peerId) const;

private:
    void insertRoute(const Route &route);

private:
    QnId m_startId;
    typedef QList<Route> RouteList;
    QHash<QnId, RouteList> m_routes;
};

#endif // ROUTE_BUILDER_H
