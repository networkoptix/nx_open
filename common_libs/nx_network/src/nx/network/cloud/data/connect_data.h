/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_API_CONNECT_DATA_H
#define NX_MEDIATOR_API_CONNECT_DATA_H

#include <list>

#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

#include "connection_method.h"
#include "connection_parameters.h"
#include "stun_message_data.h"
#include "nx/network/cloud/cloud_connect_version.h"


namespace nx {
namespace hpm {
namespace api {

/** [connection_mediator, 4.3.5] */
class NX_NETWORK_API ConnectRequest
:
    public StunRequestData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::connect;

    //TODO #ak destinationHostName MUST be unicode string (e.g., QString)
    nx::String destinationHostName;
    nx::String originatingPeerID;
    nx::String connectSessionId;
    ConnectionMethods connectionMethods;
    /** If port is zero then mediator uses source port */
    std::list<SocketAddress> udpEndpointList;
    /** if \a true, mediator does not report Connect request source address to the server peer.
        Only addresses found in \a udpEndpointList are reported
    */
    bool ignoreSourceAddress;
    CloudConnectVersion cloudConnectVersion;

    ConnectRequest();

    ConnectRequest(const ConnectRequest&) = default;
    ConnectRequest& operator=(const ConnectRequest&) = default;
    ConnectRequest(ConnectRequest&&) = default;
    ConnectRequest& operator=(ConnectRequest&&) = default;

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

class NX_NETWORK_API ConnectResponse
:
    public StunResponseData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::connect;

    std::list<SocketAddress> publicTcpEndpointList;
    std::list<SocketAddress> udpEndpointList;
    ConnectionParameters params;
    CloudConnectVersion cloudConnectVersion;

    ConnectResponse();

    ConnectResponse(const ConnectResponse&) = default;
    ConnectResponse& operator=(const ConnectResponse&) = default;
    ConnectResponse(ConnectResponse&&) = default;
    ConnectResponse& operator=(ConnectResponse&&) = default;

    /**
        \note after this method call object contents are undefined
    */
    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

}   //namespace api
}   //namespace hpm
}   //namespace nx

#endif   //NX_MEDIATOR_API_CONNECT_DATA_H
