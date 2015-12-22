/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CLOUD_CONNECTIVITY_RESOLVE_DATA_H
#define NX_CLOUD_CONNECTIVITY_RESOLVE_DATA_H

#include <cstdint>
#include <list>

#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>

#include "stun_message_data.h"


namespace nx {
namespace cc {
namespace api {

namespace ConnectionMethod
{
    enum Value
    {
        udpHolePunching = 1,
        tcpHolePunching = 2,
        proxy = 4,
        reverseConnect = 8,
    };
}

typedef std::size_t ConnectionMethods;

class NX_NETWORK_API ResolveRequest
:
    public StunMessageData
{
public:
    nx::String hostName;

    ResolveRequest();
    ResolveRequest(nx::String _hostName);

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};
 
class NX_NETWORK_API ResolveResponse
:
    public StunMessageData
{
public:
    std::list<SocketAddress> endpoints;
    ConnectionMethods connectionMethods;

    ResolveResponse();

    /*!
        \note after this method call object contents are undefined
    */
    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //cc
}   //nx

#endif  //NX_CLOUD_CONNECTIVITY_RESOLVE_DATA_H
