#ifndef ROUTE_BUILDER_H
#define ROUTE_BUILDER_H

#include <utils/network/route.h>

class QnRouteBuilder {
public:
    explicit QnRouteBuilder(const QnId &startId);

    void addConnection(const QnId &from, const QnId &to, const QString &host, quint16 port, int weight = 5);
    void removeConnection(const QnId &from, const QnId &to, const QString &host, quint16 port);

    void clear();

    QnRoute routeTo(const QnId &peerId) const;

    QHash<QnId, QnRouteList> routes() const;

private:
    typedef QPair<QnRoutePoint, int> WeightedPoint;

private:
    void insertRoute(const QnRoute &route);
    void buildRoutes(const QList<QnRoute> &initialRoutes);
    QMultiHash<QnId, WeightedPoint>::iterator findConnection(const QnId &from, const QnRoutePoint &point);

private:
    QnId m_startId;
    QHash<QnId, QnRouteList> m_routes;
    QMultiHash<QnId, WeightedPoint> m_connections;
};

#endif // ROUTE_BUILDER_H
