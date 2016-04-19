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


namespace nx {
namespace hpm {
namespace api {

/** [connection_mediator, 4.3.5] */
class NX_NETWORK_API ConnectRequest
:
    public StunMessageData
{
public:
    //TODO #ak destinationHostName MUST be unicode string (e.g., QString)
    nx::String destinationHostName;
    nx::String originatingPeerID;
    nx::String connectSessionId;
    ConnectionMethods connectionMethods;

    ConnectRequest();

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

class NX_NETWORK_API ConnectResponse
:
    public StunMessageData
{
public:
    std::list<SocketAddress> publicTcpEndpointList;
    std::list<SocketAddress> udpEndpointList;
    ConnectionParameters params;

    ConnectResponse();

    /**
        \note after this method call object contents are undefined
    */
    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx

#endif   //NX_MEDIATOR_API_CONNECT_DATA_H
