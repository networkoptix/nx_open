#pragma once

#include <vector>

#include <QtCore/QByteArray>

#include <nx/vms/api/data/data.h>
#include <nx/vms/api/analytics/settings.h>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

enum class ServerRuntimeEventType
{
    undefined = 0,
    deviceAgentSettingsMaybeChanged = 1,
    deviceFootageChanged = 2,
    analyticsStorageParametersChanged = 3,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ServerRuntimeEventType)

struct NX_VMS_API ServerRuntimeEventData: public Data
{
    ServerRuntimeEventType eventType = ServerRuntimeEventType::undefined;
    QByteArray eventData;
};

#define nx_vms_api_ServerRuntimeEventData_Fields (eventType)(eventData)

QN_FUSION_DECLARE_FUNCTIONS(ServerRuntimeEventData, (eq)(json)(ubjson), NX_VMS_API)

// Specific event payload -------------------------------------------------------------------------

struct NX_VMS_API SettingsData
{
    /**
     * Defines the scope for sequenceNumber: when the Client receives the instance with the new
     * sessionId (different to the previous one), it must reset its stored sequenceNumber.     
     */
    QnUuid sessionId;
    
    /**
     * Used to prevent older transactions from overwriting the newer ones: when the Client receives
     * the instance with sequenceNumber less that the one of the previosly received instance with
     * the same sessionId, it must discard the instance.
     */
    int64_t sequenceNumber = 0;

    QnUuid modelId;
    analytics::SettingsModel model;
    analytics::SettingsValues values;
};

#define nx_vms_api_SettingsData_Fields (sessionId)(sequenceNumber)(modelId)(model)(values)

QN_FUSION_DECLARE_FUNCTIONS(SettingsData, (json), NX_VMS_API)

struct NX_VMS_API DeviceAgentSettingsMaybeChangedData
{
    QnUuid deviceId;
    QnUuid engineId;

    SettingsData settingsData;
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

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::ServerRuntimeEventData);

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::ServerRuntimeEventType,
    (metatype)(numeric)(lexical)(debug),
    NX_VMS_API)
