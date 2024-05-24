// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_search.h"

#include <nx/fusion/model_functions.h>

#include "combined_id.h"

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearchId, (json), DeviceSearchId_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearchStatus, (csv_record)(json)(ubjson)(xml), DeviceSearchStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearchV2, (json), DeviceSearchV2_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearchIp, (json), DeviceSearchIp_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearchIpRange, (json), DeviceSearchIpRange_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearchV1, (json), DeviceSearchV1_Fields)

nx::Uuid DeviceSearchId::searchId() const
{
    return combined_id::localIdFromCombined(id);
}

nx::Uuid DeviceSearchId::serverIdFromId() const
{
    return combined_id::serverIdFromCombined(id);
}

void DeviceSearchId::setIds(const nx::Uuid& searchId, const nx::Uuid& serverId)
{
    id = combined_id::combineId(searchId, serverId);
}

DeviceSearchV1::DeviceSearchV1(DeviceSearchV2&& origin):
    DeviceSearchBase(std::move(origin))
{
    if (std::holds_alternative<DeviceSearchIp>(origin.target))
    {
        ip = std::move(std::get<DeviceSearchIp>(std::move(origin.target)).ip);
    }
    else
    {
        auto ipRange = std::get<DeviceSearchIpRange>(std::move(origin.target));
        startIp = std::move(ipRange.startIp);
        endIp = std::move(ipRange.endIp);
    }
}

void DeviceSearchBase::stripIdToLocal()
{
    if (!id.isEmpty())
        id = searchId().toString();
}

} // namespace nx::vms::api
