#pragma once

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ClientBindRequest:
    public StunRequestData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::clientBind;

    nx::String originatingPeerID;
    std::list<network::SocketAddress> tcpReverseEndpoints;

    ClientBindRequest();
    void serializeAttributes(nx::network::stun::Message* const message) override;
    bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API ClientBindResponse:
    public StunResponseData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::clientBind;

    boost::optional<network::KeepAliveOptions> tcpConnectionKeepAlive;

    ClientBindResponse();
    void serializeAttributes(nx::network::stun::Message* const message) override;
    bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
