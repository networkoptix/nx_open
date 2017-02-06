#include "resolve_domain_data.h"

#include <nx/network/stun/extension/stun_extension_types.h>


namespace nx {
namespace hpm {
namespace api {

ResolveDomainRequest::ResolveDomainRequest(nx::String domainName_)
:
    StunRequestData(kMethod),
    domainName(std::move(domainName_))
{
}

void ResolveDomainRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::extension::attrs::HostName>(domainName);
}

bool ResolveDomainRequest::parseAttributes(const nx::stun::Message& message)
{
    return readStringAttributeValue<stun::extension::attrs::HostName>(message, &domainName);
}


ResolveDomainResponse::ResolveDomainResponse(std::vector<nx::String> hostNames_)
:
    StunResponseData(kMethod),
    hostNames(std::move(hostNames_))
{
}

void ResolveDomainResponse::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute< stun::extension::attrs::HostNameList >(std::move(hostNames));
}

bool ResolveDomainResponse::parseAttributes(const nx::stun::Message& message)
{
    return readAttributeValue<stun::extension::attrs::HostNameList>(message, &hostNames);
}

} // namespace api
} // namespace hpm
} // namespace nx
