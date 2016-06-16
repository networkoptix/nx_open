/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/stun/message.h>

#include "stun_message_data.h"
#include "nx/network/cloud/cloud_connect_version.h"


namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ListenRequest
:
    public StunRequestData
{
public:
    constexpr static const stun::cc::methods::Value kMethod =
        stun::cc::methods::listen;

    nx::String systemId;
    nx::String serverId;
    CloudConnectVersion cloudConnectVersion;

    ListenRequest();

    ListenRequest(const ListenRequest&) = default;
    ListenRequest& operator=(const ListenRequest&) = default;
    ListenRequest(ListenRequest&&) = default;
    ListenRequest& operator=(ListenRequest&&) = default;

    void serializeAttributes(nx::stun::Message* const message);
    bool parseAttributes(const nx::stun::Message& message);
};

} // namespace api
} // namespace hpm
} // namespace nx
