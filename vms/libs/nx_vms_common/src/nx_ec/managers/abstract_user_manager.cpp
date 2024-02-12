// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_user_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractUserManager::getUsersSync(nx::vms::api::UserDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getUsers(std::move(handler));
        },
        outDataList);
}

Result AbstractUserManager::saveSync(const nx::vms::api::UserData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

ErrorCode AbstractUserManager::saveSync(const nx::vms::api::UserDataList& dataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(dataList, std::move(handler));
        });
}

ErrorCode AbstractUserManager::removeSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(id, std::move(handler));
        });
}

ErrorCode AbstractUserManager::getUserRolesSync(nx::vms::api::UserGroupDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getUserRoles(std::move(handler));
        },
        outDataList);
}

ErrorCode AbstractUserManager::saveUserRoleSync(const nx::vms::api::UserGroupData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            saveUserRole(data, std::move(handler));
        });
}

ErrorCode AbstractUserManager::removeUserRoleSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            removeUserRole(id, std::move(handler));
        });
}

} // namespace ec2
