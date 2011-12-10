#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include <QHostAddress>
#include <QAuthenticator>

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/video_server.h"
#include "core/resource/qnstorage.h"

class AppSessionManager;
class QnAppServerConnectionFactory;

class QN_EXPORT QnAppServerConnection
{
public:
    ~QnAppServerConnection();

    int getResourceTypes(QList<QnResourceTypePtr>& resourceTypes);

    int getResources(QList<QnResourcePtr>& resources);

    int addServer(const QnVideoServer&, QnVideoServerList& servers);
    int addCamera(const QnNetworkResource&, const QnId& serverId, QList<QnResourcePtr>& cameras);
    int addStorage(const QnStorage&);

    int getServers(QnResourceList& servers);
    int getStorages(QnResourceList& storages);

    bool isConnected() const;

    QString getLastError() const;

private:
    QnAppServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth, QnResourceFactory& resourceFactory);

private:
    QSharedPointer<AppSessionManager> m_sessionManager;
    QnResourceFactory& m_resourceFactory;
    QnVideoServerFactory m_serverFactory;

    friend class QnAppServerConnectionFactory;
};

typedef QSharedPointer<QnAppServerConnection> QnAppServerConnectionPtr;

class QN_EXPORT QnAppServerConnectionFactory
{
public:
    static void initialize(const QHostAddress& host, int port, const QAuthenticator& auth);
    static QnAppServerConnectionPtr createConnection(QnResourceFactory& resourceFactory);

private:
    static QHostAddress m_host;
    static int m_port;
    static QAuthenticator m_auth;
};

#endif // APPSERVERCONNECTIONIMPL_H
