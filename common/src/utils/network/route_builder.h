#ifndef ROUTE_BUILDER_H
#define ROUTE_BUILDER_H

#include <utils/network/route.h>

class QnRouteBuilder {
public:
    explicit QnRouteBuilder(const QnUuid &startId);

    void addConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port, int weight = 5);
    void removeConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port);

    void clear();

    QnRoute routeTo(const QnUuid &peerId) const;

    QHash<QnUuid, QnRouteList> routes() const;

private:
    typedef QPair<QnRoutePoint, int> WeightedPoint;

private:
    bool insertRoute(const QnRoute &route);
    void buildRoutes(const QList<QnRoute> &initialRoutes);
    QMultiHash<QnUuid, WeightedPoint>::iterator findConnection(const QnUuid &from, const QnRoutePoint &point);

private:
    QnUuid m_startId;
    QHash<QnUuid, QnRouteList> m_routes;
    QMultiHash<QnUuid, WeightedPoint> m_connections;
};

#endif // ROUTE_BUILDER_H
