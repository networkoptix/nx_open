// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "id_data.h"

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

    GlobalPermissions permissions;

    /**%apidoc[opt] List of roles to inherit permissions. */
    std::vector<QnUuid> parentRoleIds; //< TODO: Rename to parentGroupIds on gneral renaming.

    bool isLdap = false;

    /**%apidoc[opt] */
    QString description;

    UserRoleData() = default;
    UserRoleData(
        const QnUuid& id, const QString& name,
        GlobalPermissions permissions = {}, std::vector<QnUuid> parentRoleIds = {})
        :
        IdData(id), name(name), permissions(permissions), parentRoleIds(std::move(parentRoleIds))
    {
    }

    bool operator==(const UserRoleData& other) const = default;
    QString toString() const;
};
#define UserRoleData_Fields \
    IdData_Fields \
    (name) \
    (permissions) \
    (parentRoleIds) \
    (isLdap) \
    (description)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UserRoleData)

/**%apidoc Predefined non-editable role.
 */
struct NX_VMS_API PredefinedRoleData
{
    QString name;
    GlobalPermissions permissions;
    bool isOwner = false;

    PredefinedRoleData() = default;
    PredefinedRoleData(const QString& name, GlobalPermissions permissions, bool isOwner):
        name(name), permissions(permissions), isOwner(isOwner) {}

    bool operator==(const PredefinedRoleData& other) const = default;
};
#define PredefinedRoleData_Fields \
    (name) \
    (permissions) \
    (isOwner)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(PredefinedRoleData)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::UserRoleData)
Q_DECLARE_METATYPE(nx::vms::api::UserRoleDataList)
Q_DECLARE_METATYPE(nx::vms::api::PredefinedRoleData)
Q_DECLARE_METATYPE(nx::vms::api::PredefinedRoleDataList)
