// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_data_deprecated.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

#include "permission_converter.h"
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

std::tuple<UserData, AccessRightsData> UserDataDeprecated::toUserData() const
{
    UserData newUserData;
    static_cast<nx::vms::api::ResourceData&>(newUserData) =
        static_cast<const nx::vms::api::ResourceData&>(*this);

    if (this->isAdmin)
        newUserData.groupIds.push_back(nx::vms::api::kAdministratorsGroupId);
    if (const auto& id = permissionPresetToGroupId(this->permissions))
        newUserData.groupIds.push_back(*id);
    else
        newUserData.permissions = this->permissions;
    if (!this->userRoleId.isNull())
        newUserData.groupIds.push_back(this->userRoleId);
    for (const auto& id: this->userRoleIds)
        newUserData.groupIds.push_back(id);
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

    auto accessRights = PermissionConverter::accessRights(
        &newUserData.permissions, newUserData.id);

    return {std::move(newUserData), std::move(accessRights)};
}

std::optional<QnUuid> UserDataDeprecated::permissionPresetToGroupId(GlobalPermissions preset)
{
    switch (static_cast<GlobalPermission>(preset.toInt()))
    {
        case GlobalPermission::powerUserPermissions: return kPowerUsersGroupId;
        case GlobalPermission::advancedViewerPermissions: return kAdvancedViewersGroupId;
        case GlobalPermission::viewerPermissions: return kViewersGroupId;
        case GlobalPermission::liveViewerPermissions: return kLiveViewersGroupId;
        default: return std::nullopt;
    }
}

GlobalPermissions UserDataDeprecated::groupIdToPermissionPreset(const QnUuid& id)
{
    if (id == kPowerUsersGroupId) return GlobalPermission::powerUserPermissions;
    if (id == kAdvancedViewersGroupId) return GlobalPermission::advancedViewerPermissions;
    if (id == kViewersGroupId) return GlobalPermission::viewerPermissions;
    if (id == kLiveViewersGroupId) return GlobalPermission::liveViewerPermissions;
    return GlobalPermission::none;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserDataDeprecated, (ubjson)(json)(xml)(sql_record)(csv_record), UserDataDeprecated_Fields)

} // namespace nx::vms::api
