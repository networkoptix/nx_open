#ifndef PCP_CLIENT_H
#define PCP_CLIENT_H

#include <nx/network/socket_factory.h>
#include "utils/common/subscription.h"

#include <QtCore/QMutex>

namespace pcp {

/** PCP Mapping information */
struct Mapping
{
    SocketAddress internal;
    SocketAddress external;

    Mapping(const SocketAddress& _internal = SocketAddress(),
            const SocketAddress& _external = SocketAddress())
        : internal(_internal), external(_external) {}
};

/** PCP Client to work with multiplie network interfaces */
class Client
{
public:
    static Client& instance();

    /** Mapps @param port on @param address to the same port on NAT async */
    Guard mapPort(const SocketAddress& address);

    /** Subscribes for port mapping events */
    Guard subscribe(const std::function<void(Mapping)>& callback);

private:
    Subscription<Mapping> m_subscription;
};

} // namespace pcp

#endif // PCP_CLIENT_H
