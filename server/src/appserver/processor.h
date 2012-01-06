#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include "core/resource/resource.h"
#include "api/AppServerConnection.h"

class QnAppserverResourceProcessor : public QnResourceProcessor
{
public:
    QnAppserverResourceProcessor(const QnId& serverId);

    void processResources(const QnResourceList &resources);
private:
    QnAppServerConnectionPtr m_appServer;
    QnId m_serverId;
};

#endif //_server_appserver_processor_h_
