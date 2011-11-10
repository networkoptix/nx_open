#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include <QHostAddress>
#include <QAuthenticator>

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/video_server.h"

class AppSessionManager;

class QN_EXPORT QnAppServerConnection
{
public:
    QnAppServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth, QnResourceFactory& resourceFactory);

    ~QnAppServerConnection();

    int getResourceTypes(QList<QnResourceTypePtr>& resourceTypes);

    int getResources(QList<QnResourcePtr>& resources);

    int addServer(const QnVideoServer&, QnVideoServerList& servers);
    int addCamera(const QnNetworkResource&, const QnId& serverId, QList<QnResourcePtr>& cameras);

private:
    QSharedPointer<AppSessionManager> m_sessionManager;
    QnResourceFactory& m_resourceFactory;
    QnVideoServerFactory m_serverFactory;

};

#endif // APPSERVERCONNECTIONIMPL_H
