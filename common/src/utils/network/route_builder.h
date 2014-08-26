#ifndef ROUTE_BUILDER_H
#define ROUTE_BUILDER_H

#include <utils/network/route.h>

class QnRouteBuilder {
public:
    explicit QnRouteBuilder(const QUuid &startId);

    void addConnection(const QUuid &from, const QUuid &to, const QString &host, quint16 port, int weight = 5);
    void removeConnection(const QUuid &from, const QUuid &to, const QString &host, quint16 port);

    void clear();

    QnRoute routeTo(const QUuid &peerId) const;

    QHash<QUuid, QnRouteList> routes() const;

private:
    typedef QPair<QnRoutePoint, int> WeightedPoint;

private:
    void insertRoute(const QnRoute &route);
    void buildRoutes(const QList<QnRoute> &initialRoutes);
    QMultiHash<QUuid, WeightedPoint>::iterator findConnection(const QUuid &from, const QnRoutePoint &point);

private:
    QUuid m_startId;
    QHash<QUuid, QnRouteList> m_routes;
    QMultiHash<QUuid, WeightedPoint> m_connections;
};

#endif // ROUTE_BUILDER_H
