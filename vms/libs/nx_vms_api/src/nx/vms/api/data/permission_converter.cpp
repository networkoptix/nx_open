// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "permission_converter.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

std::map<QnUuid, AccessRights> migrateAccessRights(
    GlobalPermissions* permissions, const std::vector<QnUuid>& accessibleResources)
{
    const auto guard = nx::utils::makeScopeGuard(
        [permissions] { *permissions &= kNonDeprecatedGlobalPermissions; });

    if (permissions->testFlag(GlobalPermission::powerUser))
    {
        static const std::map<QnUuid, AccessRights> kPowerUserAccess{
            {kAllDevicesGroupId, kFullAccessRights},
            {kAllServersGroupId, kViewAccessRights},
            {kAllWebPagesGroupId, kViewAccessRights},
            {kAllVideoWallsGroupId, kViewAccessRights}};

        return kPowerUserAccess;
    }

    AccessRights accessRights = AccessRight::view;
    if (permissions->testFlag(GlobalPermission::editCameras))
        accessRights |= AccessRight::edit;
    if (permissions->testFlag(GlobalPermission::viewArchive))
        accessRights |= AccessRight::viewArchive;
    if (permissions->testFlag(GlobalPermission::exportArchive))
        accessRights |= AccessRight::exportArchive;
    if (permissions->testFlag(GlobalPermission::viewBookmarks))
        accessRights |= AccessRight::viewBookmarks;
    if (permissions->testFlag(GlobalPermission::manageBookmarks))
        accessRights |= AccessRight::manageBookmarks;
    if (permissions->testFlag(GlobalPermission::userInput))
        accessRights |= AccessRight::userInput;

    std::map<QnUuid, AccessRights> accessMap;
    if (permissions->testFlag(GlobalPermission::accessAllMedia))
    {
        accessMap.emplace(kAllDevicesGroupId, accessRights);
        accessMap.emplace(kAllServersGroupId, AccessRight::view);
        accessMap.emplace(kAllWebPagesGroupId, AccessRight::view);
    }

    if (permissions->testFlag(GlobalPermission::controlVideowall))
        accessMap.emplace(kAllVideoWallsGroupId, AccessRight::edit);

    for (const auto& id: accessibleResources)
        accessMap.emplace(id, accessRights);

    return accessMap;
}

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

void PermissionConverter::extractFromResourceAccessRights(
    const std::map<QnUuid, AccessRights>& resourceAccessRights,
    GlobalPermissions* permissions,
    std::optional<std::vector<QnUuid>>* accessibleResources)
{
    std::vector<QnUuid> temporaryAccessibleResources;
    temporaryAccessibleResources.reserve(resourceAccessRights.size());
    for (const auto& [id, resourceRight]: resourceAccessRights)
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
