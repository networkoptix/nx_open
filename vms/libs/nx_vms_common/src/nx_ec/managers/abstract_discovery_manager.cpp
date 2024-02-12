// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_discovery_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractDiscoveryManager::discoverPeerSync(const nx::Uuid& id, const nx::utils::Url& url)
{
    return detail::callSync(
        [&](auto handler)
        {
            discoverPeer(id, url, std::move(handler));
        });
}

ErrorCode AbstractDiscoveryManager::addDiscoveryInformationSync(
    const nx::Uuid& id, const nx::utils::Url& url, bool ignore)
{
    return detail::callSync(
        [&](auto handler)
        {
            addDiscoveryInformation(id, url, ignore, std::move(handler));
        });
}

ErrorCode AbstractDiscoveryManager::removeDiscoveryInformationSync(
    const nx::Uuid& id, const nx::utils::Url& url, bool ignore)
{
    return detail::callSync(
        [&](auto handler)
        {
            removeDiscoveryInformation(id, url, ignore, std::move(handler));
        });
}

ErrorCode AbstractDiscoveryManager::getDiscoveryDataSync(
    nx::vms::api::DiscoveryDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getDiscoveryData(std::move(handler));
        },
        outDataList);
}

} // namespace ec2
