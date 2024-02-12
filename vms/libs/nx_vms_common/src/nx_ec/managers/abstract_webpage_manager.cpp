// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_webpage_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractWebPageManager::getWebPagesSync(nx::vms::api::WebPageDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getWebPages(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractWebPageManager::saveSync(const nx::vms::api::WebPageData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractWebPageManager::removeSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(id, std::move(handler));
        });
}

} // namespace ec2
