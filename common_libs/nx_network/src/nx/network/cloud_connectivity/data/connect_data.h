/**********************************************************
* Dec 23, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_API_CONNECT_DATA_H
#define NX_MEDIATOR_API_CONNECT_DATA_H

#include <list>

#include <nx/network/socket_common.h>

#include "resolve_data.h"


namespace nx {
namespace cc {
namespace api {

class NX_NETWORK_API ConnectRequest
:
    public StunMessageData
{
public:
    nx::String hostName;
    nx::String peerID;
    nx::String connectionID;

    ConnectRequest();

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

class NX_NETWORK_API ConnectResponse
:
    public StunMessageData
{
public:
    std::list<SocketAddress> endpoints;

    ConnectResponse();

    /*!
        \note after this method call object contents are undefined
    */
    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //cc
}   //nx

#endif   //NX_MEDIATOR_API_CONNECT_DATA_H
