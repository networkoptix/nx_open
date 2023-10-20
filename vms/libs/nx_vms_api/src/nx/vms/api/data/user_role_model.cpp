// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_role_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(UserRoleModel, UserRoleModel_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserRoleModel, (csv_record)(json)(ubjson)(xml))

UserRoleModel::DbUpdateTypes UserRoleModel::toDbTypes() &&
{
    UserGroupData userRole;
    userRole.id = std::move(id);
    userRole.name = std::move(name);
    userRole.description = std::move(description);
    std::tie(userRole.permissions, userRole.parentGroupIds, userRole.resourceAccessRights) =
        migrateAccessRights(permissions, accessibleResources.value_or(std::vector<QnUuid>{}));
    userRole.parentGroupIds.insert(
        userRole.parentGroupIds.end(), parentGroupIds.begin(), parentGroupIds.end());
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
        std::tie(model.permissions, model.accessibleResources, std::ignore) =
            extractFromResourceAccessRights(
                baseData.permissions,
                &baseData.parentGroupIds,
                baseData.resourceAccessRights);
        model.parentGroupIds = std::move(baseData.parentGroupIds);
        result.emplace_back(std::move(model));
    }
    return result;
}

} // namespace nx::vms::api
