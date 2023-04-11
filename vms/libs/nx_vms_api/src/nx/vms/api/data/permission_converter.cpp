// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "permission_converter.h"

#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

static GlobalPermissions accessRightsToGlobalPermissions(AccessRights accessRights)
{
    constexpr std::array<std::pair<AccessRight, GlobalPermission>, 6> kLut{{
        {AccessRight::viewArchive, GlobalPermission::viewArchive},
        {AccessRight::exportArchive, GlobalPermission::exportArchive},
        {AccessRight::viewBookmarks, GlobalPermission::viewBookmarks},
        {AccessRight::manageBookmarks, GlobalPermission::manageBookmarks},
        {AccessRight::userInput, GlobalPermission::userInput},
        {AccessRight::edit, GlobalPermission::editCameras},
    }};
    GlobalPermissions result;
    for (const auto [accessRight, permission]: kLut)
    {
        if (accessRights.testFlag(accessRight))
            result.setFlag(permission);
    }
    return result;
}

AccessRightsData PermissionConverter::accessRights(
    GlobalPermissions* permissions,
    const QnUuid& id,
    const std::optional<std::vector<QnUuid>>& accessibleResources)
{
    AccessRightsData data{
        .userId = id,
        .resourceRights = migrateAccessRights(
            *permissions,
            accessibleResources.value_or(std::vector<QnUuid>())),
        .checkResourceExists = CheckResourceExists::no};
    *permissions &= kNonDeprecatedGlobalPermissions;
    return data;
}

void PermissionConverter::extractFromResourceAccessRights(
    const std::vector<AccessRightsData>& allAccessRights,
    const QnUuid& id,
    GlobalPermissions* permissions,
    std::optional<std::vector<QnUuid>>* accessibleResources)
{
    if (permissions->testFlag(GlobalPermission::powerUser))
        return;

    auto accessRights =
        nx::utils::find_if(allAccessRights, [id](auto r) { return r.userId == id; });
    if (!accessRights)
        return;

    std::vector<QnUuid> temporaryAccessibleResources;
    temporaryAccessibleResources.reserve(accessRights->resourceRights.size());
    for (const auto& [id, resourceRight]: accessRights->resourceRights)
    {
        if (id == kAllVideoWallsGroupId)
        {
            if (resourceRight.testFlag(AccessRight::edit))
                permissions->setFlag(GlobalPermission::controlVideowall);
            continue;
        }

        if (id == kAllWebPagesGroupId || id == kAllServersGroupId)
        {
            if (resourceRight.testFlag(AccessRight::view))
                permissions->setFlag(GlobalPermission::accessAllMedia);
            continue;
        }

        *permissions |= accessRightsToGlobalPermissions(resourceRight);
        if (resourceRight.testFlag(AccessRight::view))
        {
            if (id == kAllDevicesGroupId)
                permissions->setFlag(GlobalPermission::accessAllMedia);
            else
                temporaryAccessibleResources.push_back(id);
        }
    }
    if (!temporaryAccessibleResources.empty())
        *accessibleResources = std::move(temporaryAccessibleResources);
}

} // namespace nx::vms::api
