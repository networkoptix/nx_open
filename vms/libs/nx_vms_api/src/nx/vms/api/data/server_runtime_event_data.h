#pragma once

#include <vector>

#include <QtCore/QByteArray>

#include <nx/vms/api/data/data.h>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

enum class ServerRuntimeEventType
{
    undefined = 0,
    deviceAgentSettingsMaybeChanged = 1,
    deviceFootageChanged = 2,
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

struct NX_VMS_API DeviceAgentSettingsMaybeChangedData
{
    QnUuid deviceId;
    QnUuid engineId;
};

#define nx_vms_api_DeviceAgentSettingsMaybeChangedData_Fields (deviceId)(engineId)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentSettingsMaybeChangedData, (eq)(json)(ubjson), NX_VMS_API)

struct NX_VMS_API DeviceFootageChangedData
{
    std::vector<QnUuid> deviceIds;
};

#define nx_vms_api_DeviceFootageChangedData_Fields (deviceIds)
QN_FUSION_DECLARE_FUNCTIONS(DeviceFootageChangedData, (eq)(json)(ubjson), NX_VMS_API)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::ServerRuntimeEventData);

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::ServerRuntimeEventType,
    (metatype)(numeric)(lexical)(debug),
    NX_VMS_API)
