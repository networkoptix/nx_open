// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resolve_domain_data.h"

#include <nx/network/stun/extension/stun_extension_types.h>

namespace nx::hpm::api {

ResolveDomainRequest::ResolveDomainRequest(std::string domainName_):
    StunRequestData(kMethod),
    domainName(std::move(domainName_))
{
}

void ResolveDomainRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::HostName>(domainName);
}

bool ResolveDomainRequest::parseAttributes(const nx::network::stun::Message& message)
{
    return readStringAttributeValue<network::stun::extension::attrs::HostName>(message, &domainName);
}

//-------------------------------------------------------------------------------------------------

ResolveDomainResponse::ResolveDomainResponse(std::vector<std::string> hostNames_):
    StunResponseData(kMethod),
    hostNames(std::move(hostNames_))
{
}

void ResolveDomainResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::HostNameList >(std::move(hostNames));
}

bool ResolveDomainResponse::parseAttributes(const nx::network::stun::Message& message)
{
    return readAttributeValue<network::stun::extension::attrs::HostNameList>(message, &hostNames);
}

} // namespace nx::hpm::api
