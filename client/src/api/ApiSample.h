#ifndef _MAIN_H
#define _MAIN_H

#include <QString>
#include <QList>


#include "api/AppServerConnection.h"

class Client
{
public:
    Client(const QHostAddress& host, int port, const QAuthenticator& auth);
    ~Client();
    
    void run();

private:
    void requestResourceTypes();
    void ensureServerRegistered();
    void requestAllResources();
    void addCamera();

private:
    QnId m_serverId;

    QSharedPointer<QnAppServerConnection> m_appServer;
    QList<QnResourceTypePtr> m_resourceTypes;
};

#endif // _MAIN_H
