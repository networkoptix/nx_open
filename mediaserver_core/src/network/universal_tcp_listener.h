#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <network/http_connection_listener.h>


class QnUniversalTcpListener
:
    public QnHttpConnectionListener
{
public:
    QnUniversalTcpListener(
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl);
        
    void addProxySenderConnections(const SocketAddress& proxyUrl, int size);

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket) override;
};

#endif  //__UNIVERSAL_TCP_LISTENER_H__
