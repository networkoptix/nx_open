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
    public StunMessageData
{
public:
    nx::String hostName;

    ResolvePeerRequest();
    ResolvePeerRequest(nx::String _hostName);

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};
 
class NX_NETWORK_API ResolvePeerResponse
:
    public StunMessageData
{
public:
    std::list<SocketAddress> endpoints;
    ConnectionMethods connectionMethods;

    ResolvePeerResponse();

    /*!
        \note after this method call object contents are undefined
    */
    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

} // namespace api
} // namespace hpm
} // namespace nx
