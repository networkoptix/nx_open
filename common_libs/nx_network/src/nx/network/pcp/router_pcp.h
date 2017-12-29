#ifndef ROUTER_H
#define ROUTER_H

#include <nx/network/socket_factory.h>
#include <nx/utils/subscription.h>

#include <QMutex>

namespace nx {
namespace network {
namespace pcp {

class NX_NETWORK_API Router
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
} // namespace network
} // namespace nx

#endif // ROUTER_H
