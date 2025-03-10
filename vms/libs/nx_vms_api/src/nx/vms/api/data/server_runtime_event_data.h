// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <vector>

#include <QtCore/QByteArray>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>
#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::api {

enum class ServerRuntimeEventType
{
    /**%apidoc[unused] */
    undefined = 0,
    deviceAgentSettingsMaybeChanged = 1,
    deviceFootageChanged = 2,
    analyticsStorageParametersChanged = 3,
    deviceAdvancedSettingsManifestChanged = 4,

    /** Saas service was automatically disabled due to licenses issue */
    serviceDisabled = 5,

    siteHealthMessage = 6,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ServerRuntimeEventType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ServerRuntimeEventType>;
    return visitor(
        Item{ServerRuntimeEventType::undefined, ""},
        Item{ServerRuntimeEventType::deviceAgentSettingsMaybeChanged,
            "deviceAgentSettingsMaybeChanged"},
        Item{ServerRuntimeEventType::deviceFootageChanged, "deviceFootageChanged"},
        Item{ServerRuntimeEventType::analyticsStorageParametersChanged,
            "analyticsStorageParametersChanged"},
        Item{ServerRuntimeEventType::deviceAdvancedSettingsManifestChanged,
            "deviceAdvancedSettingsManifestChanged"}
    );
}


struct NX_VMS_API ServerRuntimeEventData
{
    ServerRuntimeEventType eventType = ServerRuntimeEventType::undefined;
    QByteArray eventData;
};

#define nx_vms_api_ServerRuntimeEventData_Fields (eventType)(eventData)

QN_FUSION_DECLARE_FUNCTIONS(ServerRuntimeEventData, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerRuntimeEventData, nx_vms_api_ServerRuntimeEventData_Fields)

// Specific event payload -------------------------------------------------------------------------

struct NX_VMS_API DeviceAgentSettingsMaybeChangedData
{
    nx::Uuid deviceId;
    nx::Uuid engineId;

    analytics::DeviceAgentSettingsResponse settingsData;
};

#define nx_vms_api_DeviceAgentSettingsMaybeChangedData_Fields (deviceId)(engineId)(settingsData)

NX_REFLECTION_INSTRUMENT(DeviceAgentSettingsMaybeChangedData,
    nx_vms_api_DeviceAgentSettingsMaybeChangedData_Fields)

struct NX_VMS_API DeviceFootageChangedData
{
    std::vector<nx::Uuid> deviceIds;
};

#define nx_vms_api_DeviceFootageChangedData_Fields (deviceIds)
NX_REFLECTION_INSTRUMENT(DeviceFootageChangedData,
    nx_vms_api_DeviceFootageChangedData_Fields)

struct NX_VMS_API AnalyticsStorageParametersChangedData
{
    nx::Uuid serverId;
};

#define nx_vms_api_AnalyticsStorageParametersChangedData_Fields (serverId)
NX_REFLECTION_INSTRUMENT(AnalyticsStorageParametersChangedData,
    nx_vms_api_AnalyticsStorageParametersChangedData_Fields)

struct NX_VMS_API DeviceAdvancedSettingsManifestChangedData
{
    std::set<nx::Uuid> deviceIds;
};
#define nx_vms_api_DeviceAdvancedSettingsManifestChangedData_Fields (deviceIds)
NX_REFLECTION_INSTRUMENT(DeviceAdvancedSettingsManifestChangedData,
    nx_vms_api_DeviceAdvancedSettingsManifestChangedData_Fields)

struct NX_VMS_API ServiceDisabledData
{
    EventReason reason{EventReason::none};
    std::set<nx::Uuid> deviceIds;
};
#define nx_vms_api_ServiceDisabledData_Fields (reason)(deviceIds)
NX_REFLECTION_INSTRUMENT(ServiceDisabledData,
    nx_vms_api_ServiceDisabledData_Fields)

} // namespace nx::vms::api
