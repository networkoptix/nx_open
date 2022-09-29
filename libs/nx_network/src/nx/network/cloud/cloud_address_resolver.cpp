// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_address_resolver.h"

namespace nx::network {

CloudAddressResolver::CloudAddressResolver():
    m_cloudAddressRegex(
        "(.+\\.)?[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}",
        std::regex_constants::ECMAScript | std::regex_constants::icase)
{
}

SystemError::ErrorCode CloudAddressResolver::resolve(
    const std::string_view& hostname,
    int /*ipVersion*/,
    ResolveResult* resolveResult)
{
    NX_MUTEX_LOCKER locker(&m_mutex);

    if (!isCloudHostname(hostname))
        return SystemError::hostNotFound;

    resolveResult->entries.push_back(AddressEntry(AddressType::cloud, HostAddress(hostname)));
    return SystemError::noError;
}

bool CloudAddressResolver::isCloudHostname(const std::string_view& hostname) const
{
    return std::regex_match(
        hostname.data(), hostname.data() + hostname.size(),
        m_cloudAddressRegex);
}

} // namespace nx::network
