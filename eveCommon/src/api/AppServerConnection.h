#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"

class SessionManager;

class QN_EXPORT QnAppServerConnection
{
public:
    QnAppServerConnection(const QHostAddress& host, const QAuthenticator& auth);

    ~QnAppServerConnection();

    int getResourceTypes(QList<QnResourceTypePtr>& resourceTypes);

    int getResources(QList<QnResourcePtr>& resources);

    int addServer(const QnResource&, QList<QnResourcePtr>& servers);
    int addCamera(const QnResource&, const QnId& serverId, QList<QnResourcePtr>& cameras);

private:
    QSharedPointer<SessionManager> m_sessionManager;
};

#endif // APPSERVERCONNECTIONIMPL_H
