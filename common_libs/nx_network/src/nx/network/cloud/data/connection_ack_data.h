/**********************************************************
* Jan 18, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "connection_method.h"
#include "stun_message_data.h"
#include "nx/network/cloud/cloud_connect_version.h"


namespace nx {
namespace hpm {
namespace api {

/** [connection_mediator, 4.3.8] */
class NX_NETWORK_API ConnectionAckRequest
:
    public StunRequestData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::connectionAck;

    nx::String connectSessionId;
    ConnectionMethods connectionMethods;
    std::list<SocketAddress> udpEndpointList;
    CloudConnectVersion cloudConnectVersion;

    ConnectionAckRequest();

    ConnectionAckRequest(const ConnectionAckRequest&) = default;
    ConnectionAckRequest& operator=(const ConnectionAckRequest&) = default;
    ConnectionAckRequest(ConnectionAckRequest&&) = default;
    ConnectionAckRequest& operator=(ConnectionAckRequest&&) = default;

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

}   //api
}   //hpm
}   //nx
