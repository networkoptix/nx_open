// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_role_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

#include "permission_converter.h"

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(UserRoleModel, UserRoleModel_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserRoleModel, (csv_record)(json)(ubjson)(xml))

UserRoleModel::DbUpdateTypes UserRoleModel::toDbTypes() &&
{
    UserGroupData userRole;
    userRole.id = std::move(id);
    userRole.name = std::move(name);
    userRole.description = std::move(description);
    userRole.permissions = std::move(permissions);
    userRole.resourceAccessRights = migrateAccessRights(
        &userRole.permissions, accessibleResources.value_or(std::vector<QnUuid>{}));
    return {std::move(userRole)};
}

std::vector<UserRoleModel> UserRoleModel::fromDbTypes(DbListTypes all)
{
    auto& baseList = std::get<UserGroupDataList>(all);

    std::vector<UserRoleModel> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserRoleModel model;
        model.id = std::move(baseData.id);
        model.name = std::move(baseData.name);
        model.description = std::move(baseData.description);
        model.permissions = std::move(baseData.permissions);
        PermissionConverter::extractFromResourceAccessRights(
            baseData.resourceAccessRights, &model.permissions, &model.accessibleResources);
        result.emplace_back(std::move(model));
    }
    return result;
}

} // namespace nx::vms::api
