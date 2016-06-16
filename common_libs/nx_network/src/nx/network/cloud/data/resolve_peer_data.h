#pragma once

#include <cstdint>
#include <list>

#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>

#include "connection_method.h"
#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ResolvePeerRequest
:
    public StunRequestData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::resolvePeer;

    nx::String hostName;

    ResolvePeerRequest();
    ResolvePeerRequest(nx::String _hostName);

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};
 
class NX_NETWORK_API ResolvePeerResponse
:
    public StunResponseData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::resolvePeer;

    std::list<SocketAddress> endpoints;
    ConnectionMethods connectionMethods;

    ResolvePeerResponse();

    /*!
        \note after this method call object contents are undefined
    */
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
