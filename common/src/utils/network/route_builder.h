#ifndef ROUTE_BUILDER_H
#define ROUTE_BUILDER_H

#include <utils/network/route.h>

#define DEFAULT_ROUTE_POINT_WEIGHT 5

class QnRouteBuilder {
public:
    explicit QnRouteBuilder(const QnUuid &startId);

    void addConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port, int weight = DEFAULT_ROUTE_POINT_WEIGHT);
    void removeConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port);

    QnRoute routeTo(const QnUuid &peerId, const QnUuid &via = QnUuid());

    QHash<QnUuid, QnRoute> routes() const;

    QnRoutePoint enforcedConnection() const;
    void setEnforcedConnection(const QnRoutePoint &enforcedConnection);

private:
    typedef QPair<QnRoutePoint, int> WeightedPoint;

private:
    QnRoute buildRouteTo(const QnUuid &peerId, const QnUuid &from = QnUuid());
    QMultiHash<QnUuid, WeightedPoint>::iterator findConnection(const QnUuid &from, const QnRoutePoint &point);

private:
    QnUuid m_startId;
    QHash<QnUuid, QnRoute> m_routes;
    QMultiHash<QnUuid, WeightedPoint> m_connections;
    QnRoutePoint m_enforcedConnection;
};

#endif // ROUTE_BUILDER_H
