#pragma once

#include <nx/network/socket_factory.h>
#include <nx/utils/subscription.h>

#include <QtCore/QMutex>

namespace nx {
namespace network {
namespace pcp {

/** PCP Mapping information */
struct NX_NETWORK_API Mapping
{
    SocketAddress internal;
    SocketAddress external;

    Mapping(const SocketAddress& _internal = SocketAddress(),
            const SocketAddress& _external = SocketAddress())
        : internal(_internal), external(_external) {}
};

/** PCP Client to work with multiplie network interfaces */
class NX_NETWORK_API Client
{
public:
    static Client& instance();

    /** Mapps @param port on @param address to the same port on NAT async */
    Guard mapPort(const SocketAddress& address);

    /** Subscribes for port mapping events */
    Guard subscribe(const std::function<void(Mapping)>& callback);

private:
    nx::utils::Subscription<Mapping> m_subscription;
};

} // namespace pcp
} // namespace network
} // namespace nx
