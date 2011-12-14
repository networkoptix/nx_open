#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include <QtCore/QMutex>
#include <QtCore/QUrl>

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

    QString lastError() const;

private:
    QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory);

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
    static QUrl defaultUrl();
    static void setDefaultUrl(const QUrl &url);

    static QnAppServerConnectionPtr createConnection(QnResourceFactory &resourceFactory);

private:
    QMutex m_mutex;
    QUrl m_url;
};

#endif // APPSERVERCONNECTIONIMPL_H
