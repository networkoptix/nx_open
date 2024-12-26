// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

#include "data_macros.h"
#include "resolution_data.h"

namespace nx::vms::api {

struct NX_VMS_API ConnectedDevicesRequest
{
    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be "this" to
     * refer to the current Server.
     * %example this
     */
    nx::Uuid serverId;
    std::vector<QString> deviceIds;
};
#define ConnectedDevicesRequest_Fields (serverId)(deviceIds)
NX_VMS_API_DECLARE_STRUCT_EX(ConnectedDevicesRequest, (json))
NX_REFLECTION_INSTRUMENT(ConnectedDevicesRequest, ConnectedDevicesRequest_Fields)

struct NX_VMS_API DeviceIoFilter
{
    /**%apidoc:stringArray
     * Device id(s) to get input/output state of the port (can be obtained from "id", "physicalId" or
     * "logicalId" field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain
     * Devices).
     */
    nx::vms::api::json::ValueOrArray<QString> deviceId;
};
#define DeviceIoFilter_Fields (deviceId)
NX_VMS_API_DECLARE_STRUCT_EX(DeviceIoFilter, (json))
NX_REFLECTION_INSTRUMENT(DeviceIoFilter, DeviceIoFilter_Fields)

struct NX_VMS_API IoPortState
{
    /**%apidoc Indicates if this input/output is active. */
    bool isActive = false;

    /**%apidoc Timestamp in milliseconds since epoch when information was received or written to a device. */
    std::chrono::milliseconds timestampMs{};

    bool operator==(const IoPortState&) const = default;
};
#define IoPortState_Fields (isActive)(timestampMs)
NX_VMS_API_DECLARE_STRUCT_EX(IoPortState, (json))
NX_REFLECTION_INSTRUMENT(IoPortState, IoPortState_Fields)

struct NX_VMS_API DeviceIoState
{
    nx::Uuid deviceId;
    std::map<QString, IoPortState> ports;
};
#define DeviceIoState_Fields (deviceId)(ports)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(DeviceIoState, (json))
NX_REFLECTION_INSTRUMENT(DeviceIoState, DeviceIoState_Fields)

struct NX_VMS_API IoPortUpdateRequest
{
    /**%apidoc Set this input/output active or not. */
    bool isActive = false;

    /**%apidoc[opt] Time in milliseconds to reset Device action on action update. */
    std::chrono::milliseconds autoResetTimeoutMs{};

    /**%apidoc[opt]:string
     * Target lock coordinates if supported by Device. Format '{width}x{height}'.
     */
    std::optional<ResolutionData> targetLock{};
};
#define IoPortUpdateRequest_Fields (isActive)(autoResetTimeoutMs)(targetLock)
NX_VMS_API_DECLARE_STRUCT_EX(IoPortUpdateRequest, (json))
NX_REFLECTION_INSTRUMENT(IoPortUpdateRequest, IoPortUpdateRequest_Fields)

struct NX_VMS_API DeviceIoUpdateRequest
{
    /**%apidoc
     * Device id to set input/output states of ports (can be obtained from "id", "physicalId" or
     * "logicalId" field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain
     * cameras).
     */
    QString deviceId;

    /**%apidoc
     * Port number to command map. If the port number is empty string then a default
     * Device port is used.
     */
    std::map<QString, IoPortUpdateRequest> ports;
};
#define DeviceIoUpdateRequest_Fields (deviceId)(ports)
NX_VMS_API_DECLARE_STRUCT_EX(DeviceIoUpdateRequest, (json))
NX_REFLECTION_INSTRUMENT(DeviceIoUpdateRequest, DeviceIoUpdateRequest_Fields)

} // namespace nx::vms::api
