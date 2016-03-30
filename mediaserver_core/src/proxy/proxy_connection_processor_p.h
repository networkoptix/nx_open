#ifndef __PROXY_CONNECTION_PROCESSOR_PRIV_H_
#define __PROXY_CONNECTION_PROCESSOR_PRIV_H_

#include <chrono>

#include "utils/network/socket.h"
#include "network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"
#include "media_server/settings.h"


class QnProxyConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnProxyConnectionProcessorPrivate():
        QnTCPConnectionProcessorPrivate(),
        connectTimeout(5000)
    {
    }
    virtual ~QnProxyConnectionProcessorPrivate()
    {
    }

    QSharedPointer<AbstractStreamSocket> dstSocket;
    QnUniversalTcpListener* owner;
    QUrl lastConnectedUrl;
    std::chrono::milliseconds connectTimeout;
};

#endif // __PROXY_CONNECTION_PROCESSOR_PRIV_H_
