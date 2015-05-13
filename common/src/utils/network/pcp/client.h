#ifndef PCP_CLIENT_H
#define PCP_CLIENT_H

#include <utils/network/socket_factory.h>
#include <utils/common/subscription.h>

#include <map>

namespace pcp {

/** PCP Mapping information */
struct Mapping
{
    SocketAddress internal;
    SocketAddress external;
};

/** PCP Client to work with multiplie network interfaces */
class Client
{
    Client();

public:
    static Client& instance();

    /** Mapps @param port on @param address to the same port on NAT async */
    Guard mapPort(SocketAddress address);

    /** Subscribes for port mapping events */
    Guard subscribe(const std::function<void(Mapping)>& callback);

private:
    Subscription<Mapping> m_subscription;
};

} // namespace pcp

#endif // PCP_CLIENT_H
