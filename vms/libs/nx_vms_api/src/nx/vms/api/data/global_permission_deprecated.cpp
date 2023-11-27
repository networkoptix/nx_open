// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "global_permission_deprecated.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/algorithm.h>

#include "user_data_deprecated.h"
#include "user_group_data.h"

namespace nx::vms::api {

constexpr std::array<std::pair<AccessRight, GlobalPermissionDeprecated>, 6> kLut{{
    {AccessRight::viewArchive, GlobalPermissionDeprecated::viewArchive},
    {AccessRight::exportArchive, GlobalPermissionDeprecated::exportArchive},
    {AccessRight::viewBookmarks, GlobalPermissionDeprecated::viewBookmarks},
    {AccessRight::manageBookmarks, GlobalPermissionDeprecated::manageBookmarks},
    {AccessRight::userInput, GlobalPermissionDeprecated::userInput},
    {AccessRight::edit, GlobalPermissionDeprecated::editCameras},
}};

std::tuple<GlobalPermissions, std::vector<QnUuid>, std::map<QnUuid, AccessRights>>
    migrateAccessRights(
        GlobalPermissionsDeprecated permissions,
        const std::vector<QnUuid>& accessibleResources,
        bool isOwner)
{
    if (isOwner || permissions.testFlag(GlobalPermissionDeprecated::admin))
        return {GlobalPermission::none, {isOwner ? kAdministratorsGroupId : kPowerUsersGroupId}, {}};

    std::vector<QnUuid> groups;
    if (permissions.testFlag(GlobalPermissionDeprecated::advancedViewerPermissions))
        groups = {kAdvancedViewersGroupId};
    else if (permissions.testFlag(GlobalPermissionDeprecated::viewerPermissions))
        groups = {kViewersGroupId};
    else if (permissions.testFlag(GlobalPermissionDeprecated::liveViewerPermissions))
        groups = {kLiveViewersGroupId};

    AccessRights accessRights = AccessRight::view;
    for (const auto [accessRight, permission]: kLut)
    {
        if (permissions.testFlag(permission))
            accessRights.setFlag(accessRight);
    }

    std::map<QnUuid, AccessRights> accessMap;
    if (permissions.testFlag(GlobalPermissionDeprecated::accessAllMedia))
    {
        accessMap.emplace(kAllDevicesGroupId, accessRights);
        accessMap.emplace(kAllServersGroupId, AccessRight::view);
        accessMap.emplace(kAllWebPagesGroupId, AccessRight::view);
    }

    if (permissions.testFlag(GlobalPermissionDeprecated::controlVideowall))
        accessMap.emplace(kAllVideoWallsGroupId, AccessRight::edit);

    for (const auto& id: accessibleResources)
        accessMap.emplace(id, accessRights);

    GlobalPermissions newPermissions = GlobalPermission::none;
    if (permissions.testFlag(GlobalPermissionDeprecated::viewLogs))
        newPermissions |= GlobalPermission::viewLogs;
    if (permissions.testFlag(GlobalPermissionDeprecated::userInput))
        newPermissions |= GlobalPermission::generateEvents;

    return {newPermissions, std::move(groups), std::move(accessMap)};
}

static GlobalPermissionsDeprecated accessRightsToGlobalPermissions(AccessRights accessRights)
{
    GlobalPermissionsDeprecated result;
    for (const auto [accessRight, permission]: kLut)
    {
        if (accessRights.testFlag(accessRight))
            result.setFlag(permission);
    }
    return result;
}

std::tuple<GlobalPermissionsDeprecated, std::optional<std::vector<QnUuid>>, bool>
    extractFromResourceAccessRights(
        GlobalPermissions permissions,
        std::vector<QnUuid>* groups,
        const std::map<QnUuid, AccessRights>& resourceAccessRights)
{
    GlobalPermissionsDeprecated deprecatedPermissions;

    const auto noGroupInclusions = groups->empty();
    for (auto g = groups->begin(); g != groups->end(); ++g)
    {
        if (*g == kAdministratorsGroupId)
        {
            groups->erase(g);
            return {deprecatedPermissions, /*accessibleResources*/ {}, /*isOwner*/ true};
        }

        if (*g == kPowerUsersGroupId)
        {
            groups->erase(g);
            deprecatedPermissions.setFlag(GlobalPermissionDeprecated::admin);
            return {deprecatedPermissions, /*accessibleResources*/ {}, /*isOwner*/ false};
        }

        if (*g == kAdvancedViewersGroupId)
        {
            groups->erase(g);
            deprecatedPermissions.setFlag(GlobalPermissionDeprecated::advancedViewerPermissions);
            break;
        }

        if (*g == kViewersGroupId)
        {
            groups->erase(g);
            deprecatedPermissions.setFlag(GlobalPermissionDeprecated::viewerPermissions);
            break;
        }

        if (*g == kLiveViewersGroupId)
        {
            groups->erase(g);
            deprecatedPermissions.setFlag(GlobalPermissionDeprecated::liveViewerPermissions);
            break;
        }
    }

    std::vector<QnUuid> temporaryAccessibleResources;
    temporaryAccessibleResources.reserve(resourceAccessRights.size());
    for (const auto& [id, resourceRight]: resourceAccessRights)
    {
        if (id == kAllVideoWallsGroupId)
        {
            if (resourceRight.testFlag(AccessRight::edit))
                deprecatedPermissions.setFlag(GlobalPermissionDeprecated::controlVideowall);
            continue;
        }

        if (id == kAllWebPagesGroupId || id == kAllServersGroupId)
        {
            if (resourceRight.testFlag(AccessRight::view))
                deprecatedPermissions.setFlag(GlobalPermissionDeprecated::accessAllMedia);
            continue;
        }

        deprecatedPermissions |= accessRightsToGlobalPermissions(resourceRight);
        if (id == kAllDevicesGroupId)
        {
            if (resourceRight.testFlag(AccessRight::view))
                deprecatedPermissions.setFlag(GlobalPermissionDeprecated::accessAllMedia);
            continue;
        }

        if (!id.isNull())
            temporaryAccessibleResources.push_back(id);
    }

    utils::unique_sort(temporaryAccessibleResources);

    if (permissions.testFlag(GlobalPermission::viewLogs))
        deprecatedPermissions |= GlobalPermissionDeprecated::viewLogs;
    if (permissions.testFlag(GlobalPermission::generateEvents))
        deprecatedPermissions |= GlobalPermissionDeprecated::userInput;

    if (noGroupInclusions || !UserDataDeprecated::permissionPresetToGroupId(deprecatedPermissions))
        deprecatedPermissions |= GlobalPermissionDeprecated::customUser;

    return {
        deprecatedPermissions,
        temporaryAccessibleResources.empty()
            ? std::nullopt
            : std::optional<std::vector<QnUuid>>(std::move(temporaryAccessibleResources)),
        /*isOwner*/ false};
}

} // namespace nx::vms::api
