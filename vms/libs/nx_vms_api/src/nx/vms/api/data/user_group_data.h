// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QString>

#include <nx/vms/api/types/access_rights_types.h>

#include "user_data.h"

namespace nx::vms::api {

/**%apidoc General user role information.
 * %param[readonly] id Internal user role identifier. This identifier can be used as {id} path
 *     parameter in requests related to a user role.
 */
struct NX_VMS_API UserGroupData: IdData
{
    /**%apidoc Name of the user role. */
    QString name;

    /**%apidoc[opt] */
    QString description;

    /**%apidoc[opt]
     * Only local User Groups are supposed to be created by the API.
     * %value local This Group is managed by VMS.
     * %value ldap This Group is imported from LDAP Server.
     * %value[unused] cloud Unsupported.
     * %value[unused] temporaryLocal Unsupported.
     */
    UserType type = UserType::local;

    /**%apidoc[opt] Own user global permissions. */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt] Access rights per Resource or Resource Group. */
    std::map<QnUuid, AccessRights> resourceAccessRights;

    /**%apidoc[opt] List of roles to inherit permissions. */
    std::vector<QnUuid> parentGroupIds;

    /**%apidoc[readonly] */
    nx::vms::api::UserAttributes attributes{};

    /**%apidoc[readonly] External identification data (currently used for LDAP only). */
    UserExternalId externalId;

    UserGroupData() = default;
    UserGroupData(
        const QnUuid& id, const QString& name,
        GlobalPermissions permissions = {}, std::vector<QnUuid> parentGroupIds = {});

    bool operator==(const UserGroupData& other) const = default;
    QString toString() const;
};
#define UserGroupData_Fields \
    IdData_Fields \
    (name) \
    (description) \
    (type) \
    (permissions) \
    (parentGroupIds) \
    (attributes) \
    (externalId) \
    (resourceAccessRights)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UserGroupData)

NX_VMS_API extern const QnUuid kAdministratorsGroupId; //< ex-Owners.
NX_VMS_API extern const QnUuid kPowerUsersGroupId; //< ex-Administrators.
NX_VMS_API extern const QnUuid kAdvancedViewersGroupId;
NX_VMS_API extern const QnUuid kViewersGroupId;
NX_VMS_API extern const QnUuid kLiveViewersGroupId;
NX_VMS_API extern const QnUuid kSystemHealthViewersGroupId;

NX_VMS_API extern const std::set<QnUuid> kPredefinedGroupIds;
NX_VMS_API extern const std::set<QnUuid> kAllPowerUserGroupIds;

// Predefined id for LDAP Default user group in VMS DB.
NX_VMS_API extern const QnUuid kDefaultLdapGroupId;

} // namespace nx::vms::api
