#ifndef ROUTER_H
#define ROUTER_H

#include <QtCore/QObject>
#include <QtCore/QMutex>

#include <api/abstract_connection.h>
#include <api/runtime_info_manager.h>
#include <utils/common/singleton.h>
#include <utils/network/route.h>
#include "socket_common.h"

class QnRouteBuilder;
class QnModuleFinder;
struct QnModuleInformation;
class QElapsedTimer;
class SocketAddress;

struct QnRoute
{
    SocketAddress addr; // address for physical connect
    QnUuid id;          // requested server ID
    QnUuid gatewayId;   // proxy server ID. May be null

    bool isValid() const { return !addr.isNull(); }
};

class QnRouter : public QObject, public Singleton<QnRouter> {
    Q_OBJECT
public:
    struct Endpoint {
        QnUuid id;
        QString host;
        quint16 port;

        Endpoint(const QnUuid &id, const QString &host = QString(), quint16 port = 0) : id(id), host(host), port(port) {}
        bool operator ==(const Endpoint &other) const { return id == other.id && host == other.host && port == other.port; }
    };

    explicit QnRouter(QnModuleFinder *moduleFinder, bool passive, QObject *parent = 0);
    ~QnRouter();

    QMultiHash<QnUuid, Endpoint> connections() const;
    QHash<QnUuid, QnOldRoute> routes() const;

    QnOldRoute oldRouteTo(const QnUuid &id, const QnUuid &via = QnUuid()) const;
    QnOldRoute oldRouteTo(const QString &host, quint16 port) const;

    QnUuid whoIs(const QString &host, quint16 port) const;

    QnRoutePoint enforcedConnection() const;
    void setEnforcedConnection(const QnRoutePoint &enforcedConnection);

    // todo: new routing functions below. We have to delete above functions
    QnRoute routeTo(const QnUuid &id);

signals:
    void connectionAdded(const QnUuid &discovererId, const QnUuid &peerId, const QString &host, quint16 port);
    void connectionRemoved(const QnUuid &discovererId, const QnUuid &peerId, const QString &host, quint16 port);

private slots:
    void at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const SocketAddress &address);

    void at_runtimeInfoManager_runtimeInfoAdded(const QnPeerRuntimeInfo &data);
    void at_runtimeInfoManager_runtimeInfoChanged(const QnPeerRuntimeInfo &data);
    void at_runtimeInfoManager_runtimeInfoRemoved(const QnPeerRuntimeInfo &data);

private:
    bool addConnection(const QnUuid &id, const Endpoint &endpoint);
    bool removeConnection(const QnUuid &id, const Endpoint &endpoint);

    void updateRuntimeData(bool force = false);

private:
    mutable QMutex m_mutex;
    std::weak_ptr<ec2::AbstractECConnection> m_connection;
    QScopedPointer<QnRouteBuilder> m_routeBuilder;
    QMultiHash<QnUuid, Endpoint> m_connections;
    bool m_passive;
    QTimer *m_runtimeDataUpdateTimer;
    QScopedPointer<QElapsedTimer> m_runtimeDataElapsedTimer;
};

#endif // ROUTER_H
