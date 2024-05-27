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
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BestShotV3,
    (json), BestShotV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectTrackTitle,
    (json), ObjectTrackTitle_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BaseObjectRegion,
    (json)(sql_record)(csv_record), BaseObjectRegion_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsAttribute,
    (json)(sql_record)(csv_record), AnalyticsAttribute_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectTrackV3,
    (json), ObjectTrackV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectTrackFilterFreeText,
    (json), ObjectTrackFilterFreeText_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectTrackFilter,
    (json), ObjectTrackFilter_Fields)

/*static*/ ObjectTrackV3 ObjectTrackV3::makeStub(nx::Uuid id)
{
    using namespace std::chrono_literals;

    ObjectTrackV3 result;

    result.id = id;

    result.deviceId = nx::Uuid::createUuid();
    result.objectTypeId = "nx.base.Vehicle";
    result.firstAppearanceTimeMs = 1716840924000ms;
    result.lastAppearanceTimeMs = 1716840924999ms;
    result.objectRegion.boundingBoxGrid.resize(176, 0xFF);

    ObjectTrackTitle title;
    title.text = "Example Object Track title text.";
    result.title = title;

    return result;
}

} // namespace nx::vms::api
