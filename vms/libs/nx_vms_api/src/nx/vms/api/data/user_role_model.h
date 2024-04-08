// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>

#include "global_permission_deprecated.h"
#include "user_group_data.h"

namespace nx::vms::api {

/**%apidoc User Role information object. */
struct NX_VMS_API UserRoleModel
{
    nx::Uuid id;

    /**%apidoc
     * %example User Group 1
     */
    QString name;

    /**%apidoc[opt] */
    QString description;

    /**%apidoc[opt] */
    GlobalPermissionsDeprecated permissions;

    /**%apidoc[opt] List of User Roles to inherit permissions. */
    std::vector<nx::Uuid> parentGroupIds;

    /**%apidoc[opt] List of accessible resource ids for this User Group. */
    std::optional<std::vector<nx::Uuid>> accessibleResources;

    bool operator==(const UserRoleModel& other) const = default;

    nx::Uuid getId() const { return id; }
    void setId(nx::Uuid id_) { id = std::move(id_); }

    using DbReadTypes = std::tuple<UserGroupData>;
    using DbUpdateTypes = std::tuple<UserGroupData>;
    using DbListTypes = std::tuple<UserGroupDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserRoleModel> fromDbTypes(DbListTypes data);

    static_assert(nx::utils::isCreateModelV<UserRoleModel>);
    static_assert(nx::utils::isUpdateModelV<UserRoleModel>);
};
#define UserRoleModel_Fields (id)(name)(description)(permissions)(parentGroupIds)(accessibleResources)
QN_FUSION_DECLARE_FUNCTIONS(UserRoleModel, (csv_record)(json)(ubjson)(xml), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserRoleModel, UserRoleModel_Fields)

} // namespace nx::vms::api
