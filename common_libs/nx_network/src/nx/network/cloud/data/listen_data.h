/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/stun/message.h>

#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ListenRequest
:
    public StunMessageData
{
public:
    nx::String systemId;
    nx::String serverId;

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

} // namespace api
} // namespace hpm
} // namespace nx
