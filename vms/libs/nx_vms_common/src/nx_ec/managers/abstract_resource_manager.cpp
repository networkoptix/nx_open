// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_resource_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractResourceManager::getResourceTypesSync(QnResourceTypeList* outTypeList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getResourceTypes(std::move(handler));
        },
        outTypeList);
}

ErrorCode AbstractResourceManager::setResourceStatusSync(
    const nx::Uuid& resourceId, nx::vms::api::ResourceStatus status)
{
    return detail::callSync(
        [&](auto handler)
        {
            setResourceStatus(resourceId, status, std::move(handler));
        });
}

ErrorCode AbstractResourceManager::getKvPairsSync(
    const nx::Uuid& resourceId, nx::vms::api::ResourceParamWithRefDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getKvPairs(resourceId, std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractResourceManager::getStatusListSync(
    const nx::Uuid& resourceId, nx::vms::api::ResourceStatusDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getStatusList(resourceId, std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractResourceManager::saveSync(
    const nx::vms::api::ResourceParamWithRefDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(dataList, std::move(handler));
        });
}

int AbstractResourceManager::save(
    const nx::Uuid& resourceId,
    const nx::vms::api::ResourceParamDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::ResourceParamWithRefDataList kvPairs;
    for (const auto& p: dataList)
        kvPairs.emplace_back(resourceId, p.name, p.value);
    return save(kvPairs, std::move(handler), std::move(handlerExecutor));
}

ErrorCode AbstractResourceManager::saveSync(
    const nx::Uuid& resourceId, const nx::vms::api::ResourceParamDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(resourceId, dataList, std::move(handler));
        });
}

ErrorCode AbstractResourceManager::removeSync(const nx::Uuid& resourceId)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(resourceId, std::move(handler));
        });
}

ErrorCode AbstractResourceManager::removeSync(const QVector<nx::Uuid>& resourceIds)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(resourceIds, std::move(handler));
        });
}

ErrorCode AbstractResourceManager::removeSync(const nx::vms::api::ResourceParamWithRefData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(data, std::move(handler));
        });
}

ErrorCode AbstractResourceManager::removeHardwareIdMappingSync(const nx::Uuid& resourceId)
{
    return detail::callSync(
        [&](auto handler)
        {
            removeHardwareIdMapping(resourceId, std::move(handler));
        });
}

} // namespace ec2
