// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_group_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(UserGroupModel, UserGroupModel_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserGroupModel, (csv_record)(json)(ubjson)(xml))

UserGroupModel::DbUpdateTypes UserGroupModel::toDbTypes() &&
{
    UserGroupData userRole;
    userRole.id = std::move(id);
    userRole.name = std::move(name);
    userRole.description = std::move(description);
    userRole.type = type;
    if (externalId)
        userRole.externalId = std::move(*externalId);
    userRole.attributes = std::move(attributes);
    userRole.permissions = std::move(permissions);
    userRole.parentGroupIds = std::move(parentGroupIds);
    userRole.resourceAccessRights = std::move(resourceAccessRights);
    return {std::move(userRole)};
}

std::vector<UserGroupModel> UserGroupModel::fromDbTypes(DbListTypes all)
{
    auto& baseList = std::get<UserGroupDataList>(all);
    std::vector<UserGroupModel> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserGroupModel model;
        model.id = std::move(baseData.id);
        model.name = std::move(baseData.name);
        model.description = std::move(baseData.description);
        model.type = baseData.type;
        if (!baseData.externalId.isEmpty())
            model.externalId = std::move(baseData.externalId);
        model.permissions = std::move(baseData.permissions);
        model.attributes = std::move(baseData.attributes);
        model.parentGroupIds = std::move(baseData.parentGroupIds);
        if (!baseData.resourceAccessRights.empty())
            model.resourceAccessRights = std::move(baseData.resourceAccessRights);

        result.emplace_back(std::move(model));
    }
    return result;
}

} // namespace nx::vms::api
