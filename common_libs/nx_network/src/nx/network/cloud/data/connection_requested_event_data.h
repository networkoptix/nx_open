/**********************************************************
* Jan 18, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"
#include "nx/network/cloud/cloud_connect_version.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ConnectionRequestedEvent
:
    public StunIndicationData
{
public:
    constexpr static const stun::cc::indications::Value kMethod =
        stun::cc::indications::connectionRequested;

    nx::String connectSessionId;
    nx::String originatingPeerID;
    std::list<SocketAddress> udpEndpointList;   ///< Peer UDP addresses
    ConnectionMethods connectionMethods;        ///< All requestd connection types
    ConnectionParameters params;
    CloudConnectVersion cloudConnectVersion;

    ConnectionRequestedEvent();

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

}   //api
}   //hpm
}   //nx
