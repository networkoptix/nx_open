// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AnalyticsEngineModel, (json), nx_vms_api_AnalyticsEngineModel_Fields);

AnalyticsEngineModel::AnalyticsEngineModel(AnalyticsEngineData data):
    id(std::move(data.id)),
    name(std::move(data.name)),
    integrationId(std::move(data.parentId))
{
}

AnalyticsEngineModel::DbUpdateTypes AnalyticsEngineModel::toDbTypes() &&
{
    AnalyticsEngineData result;
    result.id = id;
    result.name = name;
    result.parentId = integrationId;

    return result;
}

std::vector<AnalyticsEngineModel> AnalyticsEngineModel::fromDbTypes(DbListTypes data)
{
    std::vector<AnalyticsEngineModel> result;
    for (auto& analyticsEngineData: std::get<0>(data))
        result.push_back(AnalyticsEngineModel(analyticsEngineData));

    return result;
}

AnalyticsEngineModel AnalyticsEngineModel::fromDb(AnalyticsEngineData data)
{
    return AnalyticsEngineModel(std::move(data));
}

} // namespace nx::vms::api
