#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include "core/resource/resource.h"
#include "api/AppServerConnection.h"

class QnAppserverResourceProcessor : public QnResourceProcessor
{
public:
    QnAppserverResourceProcessor(const QnId& serverId, const QHostAddress& host, const QAuthenticator& auth, QnResourceFactory& resourceFactory);

    void processResources(const QnResourceList &resources);
private:
    QnAppServerConnection m_appServer;
    QnId m_serverId;
};

#endif //_server_appserver_processor_h_
