#pragma once

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"
#include "nx/network/cloud/cloud_connect_version.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ConnectionRequestedEvent:
    public StunIndicationData
{
public:
    constexpr static const stun::extension::indications::Value kMethod =
        stun::extension::indications::connectionRequested;

    nx::String connectSessionId;
    nx::String originatingPeerID;
    /** Peer UDP addresses. */
    std::list<SocketAddress> udpEndpointList;
    std::list<SocketAddress> tcpReverseEndpointList;
    /** All requests connection types. */
    ConnectionMethods connectionMethods;
    ConnectionParameters params;
    CloudConnectVersion cloudConnectVersion;
    bool isPersistent;

    ConnectionRequestedEvent();
    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
