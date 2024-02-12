// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_media_server_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractMediaServerManager::getServersSync(
    nx::vms::api::MediaServerDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getServers(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractMediaServerManager::getServersExSync(
    nx::vms::api::MediaServerDataExList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getServersEx(std::move(handler));
        },
        outDataList);
}

Result AbstractMediaServerManager::saveSync(const nx::vms::api::MediaServerData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractMediaServerManager::removeSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(id, std::move(handler));
        });
}

ErrorCode AbstractMediaServerManager::saveUserAttributesSync(
    const nx::vms::api::MediaServerUserAttributesDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            saveUserAttributes(dataList, std::move(handler));
        });
}

ErrorCode AbstractMediaServerManager::saveStoragesSync(
    const nx::vms::api::StorageDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            saveStorages(dataList, std::move(handler));
        });
}

ErrorCode AbstractMediaServerManager::removeStoragesSync(const nx::vms::api::IdDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            removeStorages(dataList, std::move(handler));
        });
}

Result AbstractMediaServerManager::getUserAttributesSync(
    const nx::Uuid& mediaServerId, nx::vms::api::MediaServerUserAttributesDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getUserAttributes(mediaServerId, std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractMediaServerManager::getStoragesSync(
    const nx::Uuid& mediaServerId, nx::vms::api::StorageDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getStorages(mediaServerId, std::move(handler));
        },
        outDataList);
}

} // namespace ec2
