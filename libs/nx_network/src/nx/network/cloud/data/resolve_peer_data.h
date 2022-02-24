// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>

#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>

#include "connection_method.h"
#include "stun_message_data.h"

namespace nx::hpm::api {

class NX_NETWORK_API ResolvePeerRequest:
    public StunRequestData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::resolvePeer;

    std::string hostName;

    ResolvePeerRequest(std::string _hostName = {});

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API ResolvePeerResponse:
    public StunResponseData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::resolvePeer;

    std::vector<network::SocketAddress> endpoints;
    ConnectionMethods connectionMethods;

    ResolvePeerResponse();

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
