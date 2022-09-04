// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>

#include "access_rights_data.h"
#include "user_role_data.h"

namespace nx::vms::api {

/**%apidoc User Role information object. */
struct NX_VMS_API UserRoleModel
{
    QnUuid id;

    /**%apidoc
     * %example User Group 1
     */
    QString name;

    /**%apidoc[opt] */
    QString description;

    /**%apidoc[opt] */
    GlobalPermissions permissions;

    /**%apidoc[opt] List of accessible resource ids for this User Group. */
    std::optional<std::vector<QnUuid>> accessibleResources;

    bool operator==(const UserRoleModel& other) const = default;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }

    using DbReadTypes = std::tuple<UserRoleData, AccessRightsData>;
    using DbUpdateTypes = std::tuple<UserRoleData, std::optional<AccessRightsData>>;
    using DbListTypes = std::tuple<UserRoleDataList, AccessRightsDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserRoleModel> fromDbTypes(DbListTypes data);

    static_assert(isCreateModelV<UserRoleModel>);
    static_assert(isUpdateModelV<UserRoleModel>);
};
#define UserRoleModel_Fields (id)(name)(description)(permissions)(accessibleResources)
QN_FUSION_DECLARE_FUNCTIONS(UserRoleModel, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api
