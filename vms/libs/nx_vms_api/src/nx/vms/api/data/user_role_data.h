// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "user_data.h"

#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::api {

/**%apidoc General user role information.
 * %param[readonly] id Internal user role identifier. This identifier can be used as {id} path
 *     parameter in requests related to a user role.
 */
struct NX_VMS_API UserRoleData: IdData
{
    /**%apidoc Name of the user role. */
    QString name;

    /**%apidoc[opt] */
    QString description;

    /**%apidoc[opt] Type of the user group. */
    UserType type = UserType::local;

    /**%apidoc[opt] Own user global permissions. */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt] List of roles to inherit permissions. */
    std::vector<QnUuid> parentGroupIds;

    /**%apidoc[readonly] Whether this Role comes with the System. */
    bool isPredefined = false;

    /**%apidoc[readonly] External identification data (currently used for LDAP only). */
    UserExternalId externalId;

    UserRoleData() = default;
    UserRoleData(
        const QnUuid& id, const QString& name,
        GlobalPermissions permissions = {}, std::vector<QnUuid> parentGroupIds = {});

    static UserRoleData makePredefined(
        const QnUuid& id,
        const QString& name,
        const QString& description,
        GlobalPermissions permissions);

    bool operator==(const UserRoleData& other) const = default;
    QString toString() const;

    // Predefined id for LDAP Default user role in VMS DB.
    static const QnUuid kLdapDefaultId;
};
#define UserRoleData_Fields \
    IdData_Fields \
    (name) \
    (description) \
    (type) \
    (permissions) \
    (parentGroupIds) \
    (isPredefined) \
    (externalId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UserRoleData)

} // namespace nx::vms::api

