#ifndef __PROXY_CONNECTION_PROCESSOR_PRIV_H_
#define __PROXY_CONNECTION_PROCESSOR_PRIV_H_

#include "utils/network/socket.h"
#include "utils/network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"
#include "media_server/settings.h"


class QnProxyConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnProxyConnectionProcessorPrivate():
        QnTCPConnectionProcessorPrivate(),
        connectTimeoutMs(nx_ms_conf::DEFAULT_PROXY_CONNECT_TIMEOUT_MS)
    {
    }
    virtual ~QnProxyConnectionProcessorPrivate()
    {
    }

    QSharedPointer<AbstractStreamSocket> dstSocket;
    QnUniversalTcpListener* owner;
    QUrl lastConnectedUrl;
    int connectTimeoutMs;
};

#endif // __PROXY_CONNECTION_PROCESSOR_PRIV_H_
