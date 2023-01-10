// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <vector>

#include <QtCore/QByteArray>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>

namespace nx::vms::api {

enum class ServerRuntimeEventType
{
    undefined = 0,
    deviceAgentSettingsMaybeChanged = 1,
    deviceFootageChanged = 2,
    analyticsStorageParametersChanged = 3,
    deviceAdvancedSettingsManifestChanged = 4,
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

// Specific event payload -------------------------------------------------------------------------

struct NX_VMS_API DeviceAgentSettingsMaybeChangedData
{
    QnUuid deviceId;
    QnUuid engineId;

    analytics::DeviceAgentSettingsResponse settingsData;
};

#define nx_vms_api_DeviceAgentSettingsMaybeChangedData_Fields (deviceId)(engineId)(settingsData)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentSettingsMaybeChangedData, (json), NX_VMS_API)

struct NX_VMS_API DeviceFootageChangedData
{
    std::vector<QnUuid> deviceIds;
};

#define nx_vms_api_DeviceFootageChangedData_Fields (deviceIds)
QN_FUSION_DECLARE_FUNCTIONS(DeviceFootageChangedData, (json), NX_VMS_API)

struct NX_VMS_API AnalyticsStorageParametersChangedData
{
    QnUuid serverId;
};

#define nx_vms_api_AnalyticsStorageParametersChangedData_Fields (serverId)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsStorageParametersChangedData, (json), NX_VMS_API)

struct NX_VMS_API DeviceAdvancedSettingsManifestChangedData
{
    std::set<QnUuid> deviceIds;
};
#define nx_vms_api_DeviceAdvancedSettingsManifestChangedData_Fields (deviceIds)
QN_FUSION_DECLARE_FUNCTIONS(DeviceAdvancedSettingsManifestChangedData, (json), NX_VMS_API)

} // namespace nx::vms::api

