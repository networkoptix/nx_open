// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>
#include <optional>
#include <type_traits>

#include <nx/fusion/model_functions_fwd.h>

#include "access_rights_data.h"
#include "user_data.h"
#include "user_role_data.h"

namespace nx::vms::api {

/**%apidoc User group information object.
 */
struct NX_VMS_API UserGroupModel
{
    QnUuid id;
    QString name;

    /**%apidoc:string[opt] Only local users are supposed to be created by API.
     * %value local
     * %value ldap
    */
    UserType type = UserType::local;

    /**%apidoc[opt] */
    GlobalPermissions permissions;

    /**%apidoc[opt] List of groups to inherit permissions. */
    std::vector<QnUuid> parentGroupIds;

    /**%apidoc[opt] List of accessible resource ids for the user group. */
    std::optional<std::vector<QnUuid>> accessibleResources;

    bool operator==(const UserGroupModel& other) const = default;

    using DbReadTypes = std::tuple<UserRoleData, AccessRightsData>;
    using DbUpdateTypes = std::tuple<UserRoleData, std::optional<AccessRightsData>>;
    using DbListTypes = std::tuple<UserRoleDataList, AccessRightsDataList>;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }

    static_assert(isCreateModelV<UserGroupModel>);
    static_assert(isUpdateModelV<UserGroupModel>);

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserGroupModel> fromDbTypes(DbListTypes data);
};

#define UserGroupModel_Fields (id)(name)(type)(permissions)(parentGroupIds)(accessibleResources)

QN_FUSION_DECLARE_FUNCTIONS(UserGroupModel, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api
