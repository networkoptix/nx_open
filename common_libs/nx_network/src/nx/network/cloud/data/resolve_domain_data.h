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

class NX_NETWORK_API ResolveDomainRequest:
    public StunRequestData
{
public:
    constexpr static const stun::extension::methods::Value kMethod =
        stun::extension::methods::resolveDomain;

    nx::String domainName;

    ResolveDomainRequest(nx::String domainName_ = {});

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

class NX_NETWORK_API ResolveDomainResponse:
    public StunResponseData
{
public:
    constexpr static const stun::extension::methods::Value kMethod =
        stun::extension::methods::resolveDomain;

    std::vector<nx::String> hostNames;

    ResolveDomainResponse(std::vector<nx::String> hostNames_ = {});

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
