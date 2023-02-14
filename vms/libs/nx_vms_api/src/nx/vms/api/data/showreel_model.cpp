// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ShowreelModel, (json), ShowreelModel_Fields)

ShowreelModel::DbUpdateTypes ShowreelModel::toDbTypes() &&
{
    return {std::move(*this)};
}

std::vector<ShowreelModel> ShowreelModel::fromDbTypes(DbListTypes all)
{
    auto& baseList = std::get<LayoutTourDataList>(all);
    std::vector<ShowreelModel> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        ShowreelModel model;
        model.LayoutTourData::operator=(std::move(baseData));
        result.emplace_back(std::move(model));
    }
    return result;
}

} // namespace nx::vms::api
