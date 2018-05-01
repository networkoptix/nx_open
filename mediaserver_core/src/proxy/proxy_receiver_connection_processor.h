#pragma once

#include "network/universal_request_processor.h"
#include "proxy_connection.h"
#include <nx/mediaserver/server_module_aware.h>

class QnProxyReceiverConnection: 
    public nx::mediaserver::ServerModuleAware, 
    public QnTCPConnectionProcessor
{
public:
    QnProxyReceiverConnection(
        QnMediaServerModule* serverModule,
        QSharedPointer<nx::network::AbstractStreamSocket> socket, QnHttpConnectionListener* owner);

protected:
    virtual void run();
};
