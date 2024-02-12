// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_videowall_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractVideowallManager::getVideowallsSync(
    nx::vms::api::VideowallDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getVideowalls(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractVideowallManager::saveSync(const nx::vms::api::VideowallData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractVideowallManager::removeSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(id, std::move(handler));
        });
}

ErrorCode AbstractVideowallManager::sendControlMessageSync(
    const nx::vms::api::VideowallControlMessageData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            sendControlMessage(data, std::move(handler));
        });
}

} // namespace ec2
