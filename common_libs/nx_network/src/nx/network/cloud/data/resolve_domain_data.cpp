#include "resolve_domain_data.h"

#include <nx/network/stun/cc/custom_stun.h>


namespace nx {
namespace hpm {
namespace api {

ResolveDomainRequest::ResolveDomainRequest(nx::String domainName_)
:
    domainName(std::move(domainName_))
{
}

void ResolveDomainRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::HostName>(domainName);
}

bool ResolveDomainRequest::parse(const nx::stun::Message& message)
{
    return readStringAttributeValue<stun::cc::attrs::HostName>(message, &domainName);
}


ResolveDomainResponse::ResolveDomainResponse(std::vector<nx::String> hostNames_)
:
    hostNames(std::move(hostNames_))
{
}

void ResolveDomainResponse::serialize(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::HostNameList >(std::move(hostNames));
}

bool ResolveDomainResponse::parse(const nx::stun::Message& message)
{
    return readAttributeValue<stun::cc::attrs::HostNameList>(message, &hostNames);
}

} // namespace api
} // namespace hpm
} // namespace nx
