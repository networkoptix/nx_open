#ifndef ROUTER_H
#define ROUTER_H

#include "utils/network/socket_factory.h"
#include "utils/common/subscription.h"

#include <QMutex>

namespace pcp {

class Router
{
public:
    Router(const HostAddress& address);

    struct Mapping
    {
        SocketAddress internal;
        SocketAddress external;

        QByteArray nonce;
        quint32 lifeTime;
    };

    void mapPort(Mapping& mapping);

protected:
    static QByteArray makeMapRequest(Mapping& mapping);
    static bool parseMapResponse(const QByteArray& response, Mapping& mapping);

private:
    const SocketAddress m_serverAddress;

    QMutex m_mutex;
    std::unique_ptr<AbstractDatagramSocket> m_clientSocket;
    std::unique_ptr<AbstractDatagramSocket> m_serverSocket;
};

} // namespace pcp

#endif // ROUTER_H
