#ifndef ROUTE_BUILDER_H
#define ROUTE_BUILDER_H

#include <utils/network/route.h>

class QnRouteBuilder {
public:
    explicit QnRouteBuilder(const QnUuid &startId);

    void addConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port, int weight = 5);
    void removeConnection(const QnUuid &from, const QnUuid &to, const QString &host, quint16 port);

    void clear();

    QnRoute routeTo(const QnUuid &peerId);

    QHash<QnUuid, QnRoute> routes() const;

private:
    typedef QPair<QnRoutePoint, int> WeightedPoint;

private:
    QnRoute buildRouteTo(const QnUuid &peerId);
    QMultiHash<QnUuid, WeightedPoint>::iterator findConnection(const QnUuid &from, const QnRoutePoint &point);

private:
    QnUuid m_startId;
    QHash<QnUuid, QnRoute> m_routes;
    QMultiHash<QnUuid, WeightedPoint> m_connections;
};

#endif // ROUTE_BUILDER_H
