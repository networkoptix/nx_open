// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_data_deprecated.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

#include "user_group_data.h"

namespace nx::vms::api {

void UserDataDeprecated::fillId()
{
    // ATTENTION: This logic is similar to QnUserResource::fillId().
    if (isCloud)
    {
        if (!email.isEmpty())
            id = QnUuid::fromArbitraryData(email);
        else
            id = QnUuid();
    }
    else
    {
        id = QnUuid::createUuid();
    }
}

UserData UserDataDeprecated::toUserData() const
{
    UserData newUserData;
    static_cast<nx::vms::api::ResourceData&>(newUserData) =
        static_cast<const nx::vms::api::ResourceData&>(*this);

    newUserData.email = this->email;
    newUserData.digest = this->digest;
    newUserData.hash = this->hash;
    newUserData.cryptSha512Hash = this->cryptSha512Hash;
    newUserData.isEnabled = this->isEnabled;
    if (this->isCloud)
        newUserData.type = nx::vms::api::UserType::cloud;
    else if (this->isLdap)
        newUserData.type = nx::vms::api::UserType::ldap;
    else
        newUserData.type = nx::vms::api::UserType::local;
    newUserData.fullName = this->fullName;
    std::tie(newUserData.permissions, newUserData.groupIds, newUserData.resourceAccessRights) =
        migrateAccessRights(this->permissions, /*accessibleResources*/ {}, isAdmin);

    if (this->isAdmin)
        newUserData.attributes = nx::vms::api::UserAttribute::readonly;
    if (!this->userRoleId.isNull())
        newUserData.groupIds.push_back(this->userRoleId);
    for (const auto& id: this->userRoleIds)
        newUserData.groupIds.push_back(id);

    return {std::move(newUserData)};
}

std::optional<QnUuid> UserDataDeprecated::permissionPresetToGroupId(GlobalPermissionsDeprecated preset)
{
    if (preset == GlobalPermissionDeprecated::admin
        || preset == GlobalPermissionDeprecated::adminPermissions)
    {
        return kPowerUsersGroupId;
    }

    if (preset == GlobalPermissionDeprecated::advancedViewerPermissions)
        return kAdvancedViewersGroupId;

    if (preset == GlobalPermissionDeprecated::viewerPermissions)
        return kViewersGroupId;

    if (preset == GlobalPermissionDeprecated::liveViewerPermissions)
        return kLiveViewersGroupId;

    return std::nullopt;
}

GlobalPermissionsDeprecated UserDataDeprecated::groupIdToPermissionPreset(const QnUuid& id)
{
    if (id == kAdministratorsGroupId)
        return GlobalPermissionDeprecated::adminPermissions;

    if (id == kPowerUsersGroupId)
        return GlobalPermissionDeprecated::adminPermissions;

    if (id == kAdvancedViewersGroupId)
        return GlobalPermissionDeprecated::advancedViewerPermissions;

    if (id == kViewersGroupId)
        return GlobalPermissionDeprecated::viewerPermissions;

    if (id == kLiveViewersGroupId)
        return GlobalPermissionDeprecated::liveViewerPermissions;

    return GlobalPermissionDeprecated::none;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserDataDeprecated, (ubjson)(json)(xml)(sql_record)(csv_record), UserDataDeprecated_Fields)

} // namespace nx::vms::api
