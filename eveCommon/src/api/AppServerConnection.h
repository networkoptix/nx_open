#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include <QHostAddress>
#include <QAuthenticator>

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/server.h"

class SessionManager;

class QN_EXPORT QnAppServerConnection
{
public:
    QnAppServerConnection(const QHostAddress& host, const QAuthenticator& auth, QnResourceFactory& resourceFactory);

    ~QnAppServerConnection();

    int getResourceTypes(QList<QnResourceTypePtr>& resourceTypes);

    int getResources(QList<QnResourcePtr>& resources);

    int addServer(const QnServer&, QList<QnServerPtr>& servers);
    int addCamera(const QnNetworkResource&, const QnId& serverId, QList<QnResourcePtr>& cameras);

private:
    QSharedPointer<SessionManager> m_sessionManager;
    QnResourceFactory& m_resourceFactory;

};

#endif // APPSERVERCONNECTIONIMPL_H
