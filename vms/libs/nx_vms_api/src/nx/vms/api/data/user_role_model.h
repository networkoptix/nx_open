// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>
#include <optional>
#include <type_traits>

#include <nx/fusion/model_functions_fwd.h>

#include "access_rights_data.h"
#include "user_role_data.h"

namespace nx::vms::api {

/**%apidoc User role information object.
 */
struct NX_VMS_API UserRoleModel: UserRoleData
{
    bool operator==(const UserRoleModel& other) const = default;

    /**%apidoc[opt] List of accessible resource ids for the user role. */
    std::optional<std::vector<QnUuid>> accessibleResources;

    using DbReadTypes = std::tuple<UserRoleData, AccessRightsData>;
    using DbUpdateTypes = std::tuple<UserRoleData, std::optional<AccessRightsData>>;
    using DbListTypes = std::tuple<UserRoleDataList, AccessRightsDataList>;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }
    static_assert(isCreateModelV<UserRoleModel>);
    static_assert(isUpdateModelV<UserRoleModel>);

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserRoleModel> fromDbTypes(DbListTypes data);
};

#define UserRoleModel_Fields UserRoleData_Fields (accessibleResources)
QN_FUSION_DECLARE_FUNCTIONS(UserRoleModel, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api
