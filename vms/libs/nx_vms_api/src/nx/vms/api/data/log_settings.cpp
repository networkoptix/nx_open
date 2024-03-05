// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

void LogSettings::addPredefinedFilters(std::set<nx::log::Filter> filters)
{
    if (filters.empty())
        return;

    for (const auto& filter: filters)
        predefinedFilters.emplace_back(filter.toString());
}

void LogSettings::addCustomFilters(nx::log::LevelFilters filters)
{
    if (filters.empty())
        return;

    for (const auto& [filter, level]: filters)
        customFilters.emplace_back(LevelFilter{filter.toString(), level});
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LevelFilter, (json), LevelFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LogSettings, (json), LogSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerLogSettings, (json), ServerLogSettings_Fields)

} // namespace nx::vms::api
