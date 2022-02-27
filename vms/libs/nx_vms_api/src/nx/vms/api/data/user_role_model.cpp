// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_role_model.h"

#include <nx/utils/std/algorithm.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/json/uuid_mover.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT(UserRoleModel, UserRoleModel_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserRoleModel, (csv_record)(json)(ubjson)(xml))

UserRoleModel::DbUpdateTypes UserRoleModel::toDbTypes() &&
{
    std::optional<AccessRightsData> accessRights;
    if (accessibleResources)
    {
        AccessRightsData data;
        data.userId = getId();
        data.resourceIds = *std::move(accessibleResources);
        data.checkResourceExists = CheckResourceExists::no;
        accessRights = std::move(data);
    }
    return {std::move(*this), std::move(accessRights)};
}

std::vector<UserRoleModel> UserRoleModel::fromDbTypes(DbListTypes all)
{
    auto& baseList = std::get<UserRoleDataList>(all);
    std::vector<UserRoleModel> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserRoleModel model;
        model.id = std::move(baseData.id);
        model.name = std::move(baseData.name);
        model.permissions = std::move(baseData.permissions);

        auto accessRights = nx::utils::find_if(
            std::get<AccessRightsDataList>(all),
            [&](const auto& accessRights) { return accessRights.userId == model.id; });
        if (accessRights)
            model.accessibleResources = std::move(accessRights->resourceIds);

        result.emplace_back(std::move(model));
    }
    return result;
}

} // namespace api
} // namespace vms
} // namespace nx
