// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_layout_tour_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractLayoutTourManager::getLayoutToursSync(
    nx::vms::api::LayoutTourDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getLayoutTours(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractLayoutTourManager::saveSync(const nx::vms::api::LayoutTourData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractLayoutTourManager::removeSync(const QnUuid& tourId)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(tourId, std::move(handler));
        });
}

} // namespace ec2
