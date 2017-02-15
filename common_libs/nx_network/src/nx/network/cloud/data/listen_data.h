#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/cloud/cloud_connect_version.h>
#include <nx/network/cloud/cloud_connect_options.h>
#include <nx/network/stun/message.h>

#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API ListenRequest:
    public StunRequestData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::listen;

    // TODO: #mux Remove systemId and serverId as redandant.
    // Every server message is signed up with system id, server id and message integrity based on
    // authentification key by MediatorServerConnection.
    nx::String systemId;
    nx::String serverId;
    CloudConnectVersion cloudConnectVersion;

    ListenRequest();
    void serializeAttributes(nx::stun::Message* const message);
    bool parseAttributes(const nx::stun::Message& message);
};

class NX_NETWORK_API ListenResponse:
    public StunResponseData
{
public:
    constexpr static const auto kMethod = stun::extension::methods::listen;

    boost::optional<KeepAliveOptions> tcpConnectionKeepAlive;
    CloudConnectOptions cloudConnectOptions;

    ListenResponse();
    void serializeAttributes(nx::stun::Message* const message);
    bool parseAttributes(const nx::stun::Message& message);
};

} // namespace api
} // namespace hpm
} // namespace nx
