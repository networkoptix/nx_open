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
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::clientBind;

    nx::String originatingPeerID;
    std::list<SocketAddress> tcpReverseEndpoints;

    ClientBindRequest();
    void serializeAttributes(nx::stun::Message* const message) override;
    bool parseAttributes(const nx::stun::Message& message) override;
};

class NX_NETWORK_API ClientBindResponse:
    public StunResponseData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::clientBind;

    boost::optional<KeepAliveOptions> tcpConnectionKeepAlive;

    ClientBindResponse();
    void serializeAttributes(nx::stun::Message* const message) override;
    bool parseAttributes(const nx::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
