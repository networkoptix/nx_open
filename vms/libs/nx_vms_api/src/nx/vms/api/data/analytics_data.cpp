// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const QString AnalyticsPluginData::kResourceTypeName("AnalyticsPlugin");
const nx::Uuid AnalyticsPluginData::kResourceTypeId =
    ResourceData::getFixedTypeId(AnalyticsPluginData::kResourceTypeName);

const QString AnalyticsEngineData::kResourceTypeName("AnalyticsEngine");
const nx::Uuid AnalyticsEngineData::kResourceTypeId =
    ResourceData::getFixedTypeId(AnalyticsEngineData::kResourceTypeName);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsPluginData,
    (ubjson)(xml)(json)(sql_record)(csv_record), AnalyticsPluginData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEngineData,
    (ubjson)(xml)(json)(sql_record)(csv_record), AnalyticsEngineData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginInfo,
    (ubjson)(xml)(json)(sql_record)(csv_record), PluginInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginResourceBindingInfo,
    (ubjson)(xml)(json)(sql_record)(csv_record), PluginResourceBindingInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PluginInfoEx,
    (ubjson)(xml)(json)(sql_record)(csv_record), PluginInfoEx_Fields)

} // namespace nx::vms::api
