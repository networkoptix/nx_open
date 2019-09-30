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

QString toString(PluginInfo::Optionality value)
{
    return QnLexical::serialized(value);
}

QString toString(PluginInfo::Status value)
{
    return QnLexical::serialized(value);
}

QString toString(PluginInfo::Error value)
{
    return QnLexical::serialized(value);
}

QString toString(PluginInfo::MainInterface value)
{
    return QnLexical::serialized(value);
}

} // namespace nx::vms::api

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginInfo, Optionality,
    (nx::vms::api::PluginInfo::Optionality::nonOptional, "nonOptional")
    (nx::vms::api::PluginInfo::Optionality::optional, "optional")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginInfo, Status,
    (nx::vms::api::PluginInfo::Status::loaded, "loaded")
    (nx::vms::api::PluginInfo::Status::notLoadedBecauseOfError, "notLoadedBecauseOfError")
    (nx::vms::api::PluginInfo::Status::notLoadedBecauseOfBlackList, "notLoadedBecauseOfBlackList")
    (nx::vms::api::PluginInfo::Status::notLoadedBecauseOptional, "notLoadedBecauseOptional")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginInfo, Error,
    (nx::vms::api::PluginInfo::Error::noError, "noError")
    (nx::vms::api::PluginInfo::Error::cannotLoadLibrary, "cannotLoadLibrary")
    (nx::vms::api::PluginInfo::Error::invalidLibrary, "invalidLibrary")
    (nx::vms::api::PluginInfo::Error::libraryFailure, "libraryFailure")
    (nx::vms::api::PluginInfo::Error::badManifest, "badManifest")
    (nx::vms::api::PluginInfo::Error::unsupportedVersion, "unsupportedVersion")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::PluginInfo, MainInterface,
    (nx::vms::api::PluginInfo::MainInterface::undefined, "undefined")
    (nx::vms::api::PluginInfo::MainInterface::nxpl_PluginInterface, "nxpl_PluginInterface")
    (nx::vms::api::PluginInfo::MainInterface::nxpl_Plugin, "nxpl_Plugin")
    (nx::vms::api::PluginInfo::MainInterface::nxpl_Plugin2, "nxpl_Plugin2")
    (nx::vms::api::PluginInfo::MainInterface::nx_sdk_IPlugin, "nx_sdk_IPlugin")
    (nx::vms::api::PluginInfo::MainInterface::nx_sdk_analytics_IPlugin,
        "nx_sdk_analytics_IPlugin")
)
