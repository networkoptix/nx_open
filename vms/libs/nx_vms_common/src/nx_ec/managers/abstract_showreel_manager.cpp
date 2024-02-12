// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_showreel_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractShowreelManager::getShowreelsSync(
    nx::vms::api::ShowreelDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getShowreels(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractShowreelManager::saveSync(const nx::vms::api::ShowreelData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractShowreelManager::removeSync(const nx::Uuid& tourId)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(tourId, std::move(handler));
        });
}

} // namespace ec2
