// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>

#include "connection_method.h"
#include "stun_message_data.h"

namespace nx::hpm::api {

class NX_NETWORK_API ResolveDomainRequest:
    public StunRequestData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::resolveDomain;

    std::string domainName;

    ResolveDomainRequest(std::string domainName_ = {});

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API ResolveDomainResponse:
    public StunResponseData
{
public:
    constexpr static const network::stun::extension::methods::Value kMethod =
        network::stun::extension::methods::resolveDomain;

    std::vector<std::string> hostNames;

    ResolveDomainResponse(std::vector<std::string> hostNames_ = {});

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace nx::hpm::api
