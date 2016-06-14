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
    public StunMessageData
{
public:
    nx::String connectSessionId;
    ConnectionMethods connectionMethods;
    std::list<SocketAddress> udpEndpointList;
    CloudConnectVersion cloudConnectVersion;

    ConnectionAckRequest();

    ConnectionAckRequest(const ConnectionAckRequest&) = default;
    ConnectionAckRequest& operator=(const ConnectionAckRequest&) = default;
    ConnectionAckRequest(ConnectionAckRequest&&) = default;
    ConnectionAckRequest& operator=(ConnectionAckRequest&&) = default;

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
