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

class NX_NETWORK_API ResolveDomainRequest
:
    public StunMessageData
{
public:
    nx::String domainName;

    ResolveDomainRequest(nx::String domainName_ = {});

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};
 
class NX_NETWORK_API ResolveDomainResponse
:
    public StunMessageData
{
public:
    std::vector<nx::String> hostNames;

    ResolveDomainResponse(std::vector<nx::String> hostNames_ = {});

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

} // namespace api
} // namespace hpm
} // namespace nx
