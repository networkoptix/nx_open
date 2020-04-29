#include "server_runtime_event_data.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, ServerRuntimeEventType,
    (nx::vms::api::ServerRuntimeEventType::undefined, "")
    (nx::vms::api::ServerRuntimeEventType::deviceAgentSettingsMaybeChanged,
        "deviceAgentSettingsMaybeChanged")
    (nx::vms::api::ServerRuntimeEventType::deviceFootageChanged, "deviceFootageChanged")
    (nx::vms::api::ServerRuntimeEventType::analyticsStorageParametersChanged,
        "analyticsStorageParametersChanged"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::ServerRuntimeEventType, (numeric)(debug))

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ServerRuntimeEventData,
    (eq)(json)(ubjson),
    nx_vms_api_ServerRuntimeEventData_Fields,
    (optional, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceAgentSettingsMaybeChangedData,
    (json),
    nx_vms_api_DeviceAgentSettingsMaybeChangedData_Fields,
    (optional, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceFootageChangedData,
    (json),
    nx_vms_api_DeviceFootageChangedData_Fields,
    (optional, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AnalyticsStorageParametersChangedData,
    (json),
    nx_vms_api_AnalyticsStorageParametersChangedData_Fields,
    (optional, true))

} // namespace nx::vms::api
