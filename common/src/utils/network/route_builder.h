#ifndef ROUTE_BUILDER_H
#define ROUTE_BUILDER_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QHostAddress;

class QnRouteBuilder : public QObject {
    Q_OBJECT
public:

    struct RoutePoint {
        QnId peerId;
        QHostAddress address;
        quint16 port;

        RoutePoint(const QnId &peerId, const QHostAddress &address, quint16 port) :
            peerId(peerId), address(address), port(port)
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

    explicit QnRouteBuilder(const QnId &startId, QObject *parent = 0);

    void addConnection(const QnId &from, const QnId &to, const QHostAddress &address, quint16 port, int weight = 5);
    void removeConnection(const QnId &from, const QnId &to, const QHostAddress &address, quint16 port);

    Route routeTo(const QnId &peerId) const;

private:
    void insertRoute(const Route &route);

private:
    QnId m_startId;
    typedef QList<Route> RouteList;
    QHash<QnId, RouteList> m_routes;
};

#endif // ROUTE_BUILDER_H
