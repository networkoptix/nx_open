#include "analytics_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const QString AnalyticsPluginData::kResourceTypeName("AnalyticsPlugin");
const QnUuid AnalyticsPluginData::kResourceTypeId =
    ResourceData::getFixedTypeId(AnalyticsPluginData::kResourceTypeName);

const QString AnalyticsEngineData::kResourceTypeName("AnalyticsEngine");
const QnUuid AnalyticsEngineData::kResourceTypeId =
    ResourceData::getFixedTypeId(AnalyticsEngineData::kResourceTypeName);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AnalyticsPluginData)(AnalyticsEngineData)(PluginModuleData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)
} // namespace nx::vms::api

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginModuleData, LoadingType,
    (nx::vms::api::PluginModuleData::LoadingType::normal, "normal")
    (nx::vms::api::PluginModuleData::LoadingType::optional, "optional"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginModuleData, Status,
    (nx::vms::api::PluginModuleData::Status::loadedNormally, "loadedNormally")
    (nx::vms::api::PluginModuleData::Status::notLoadedBecauseOfError, "notLoadedBecauseOfError")
    (nx::vms::api::PluginModuleData::Status::notLoadedBecauseOfBlackList, "notLoadedBecauseOfBlackList")
    (nx::vms::api::PluginModuleData::Status::notLoadedBecausePluginIsOptional, "notLoadedBecausePluginIsOptional"))
