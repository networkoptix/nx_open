// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_layout_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractLayoutManager::getLayoutsSync(nx::vms::api::LayoutDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getLayouts(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractLayoutManager::saveSync(const nx::vms::api::LayoutData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractLayoutManager::removeSync(const nx::Uuid& layoutId)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(layoutId, std::move(handler));
        });
}

} // namespace ec2
