#ifndef __PROXY_CONNECTION_PROCESSOR_PRIV_H_
#define __PROXY_CONNECTION_PROCESSOR_PRIV_H_

#include "utils/network/socket.h"
#include "utils/network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"

class QnProxyConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnProxyConnectionProcessorPrivate():
        QnTCPConnectionProcessorPrivate()
    {
    }
    virtual ~QnProxyConnectionProcessorPrivate()
    {
    }

    QSharedPointer<AbstractStreamSocket> dstSocket;
    QnUniversalTcpListener* owner;
    QUrl lastConnectedUrl;
};

#endif // __PROXY_CONNECTION_PROCESSOR_PRIV_H_
