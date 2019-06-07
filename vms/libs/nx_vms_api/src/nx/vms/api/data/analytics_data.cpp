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
    (AnalyticsPluginData)(AnalyticsEngineData)(PluginInfo),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace nx::vms::api

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginInfo, Optionality,
    (nx::vms::api::PluginInfo::Optionality::nonOptional, "nonOptional")
    (nx::vms::api::PluginInfo::Optionality::optional, "optional"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginInfo, Status,
    (nx::vms::api::PluginInfo::Status::loaded, "loaded")
    (nx::vms::api::PluginInfo::Status::notLoadedBecauseOfError, "notLoadedBecauseOfError")
    (nx::vms::api::PluginInfo::Status::notLoadedBecauseOfBlackList, "notLoadedBecauseOfBlackList")
    (nx::vms::api::PluginInfo::Status::notLoadedBecauseOptional, "notLoadedBecauseOptional"))
