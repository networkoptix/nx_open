#ifndef __BACK_PROXY_CONNECTION_PROCESSOR_H__
#define __BACK_PROXY_CONNECTION_PROCESSOR_H__

#include "network/universal_request_processor.h"
#include "proxy_connection.h"

class QnProxyReceiverConnection: public QnTCPConnectionProcessor
{
public:
    QnProxyReceiverConnection(
        QSharedPointer<nx::network::AbstractStreamSocket> socket, QnHttpConnectionListener* owner);

protected:
    virtual void run();
};

#endif // __BACK_PROXY_CONNECTION_PROCESSOR_H__
