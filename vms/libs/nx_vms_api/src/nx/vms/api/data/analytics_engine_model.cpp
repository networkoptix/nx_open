// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EngineModel, (json), nx_vms_api_analytics_EngineModel_Fields);

EngineModel::EngineModel(AnalyticsEngineData data):
    id(std::move(data.id)),
    name(std::move(data.name)),
    integrationId(std::move(data.parentId))
{
}

EngineModel::DbUpdateTypes EngineModel::toDbTypes() &&
{
    AnalyticsEngineData result;
    result.id = id;
    result.name = name;
    result.parentId = integrationId;

    return result;
}

std::vector<EngineModel> EngineModel::fromDbTypes(DbListTypes data)
{
    std::vector<EngineModel> result;
    for (auto& analyticsEngineData: std::get<0>(data))
        result.push_back(EngineModel(analyticsEngineData));

    return result;
}

EngineModel EngineModel::fromDb(AnalyticsEngineData data)
{
    return EngineModel(std::move(data));
}

} // namespace nx::vms::api::analytics
