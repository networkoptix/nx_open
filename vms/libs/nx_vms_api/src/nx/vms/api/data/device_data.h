// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"
#include "id_data.h"

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

struct NX_VMS_API IoPortData
{
    /**%apidoc:string
     * Device id to get input/output state of the port (can be obtained from "id", "physicalId" or
     * "logicalId" field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain
     * cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc Port number. */
    QString port;

    const IoPortData& getId() const { return *this; }
    bool operator==(const IoPortData&) const = default;
};
#define IoPortData_Fields (deviceId)(port)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(IoPortData, (json))

struct NX_VMS_API IoStateResponse: IoPortData
{
    /**%apidoc Indicates if this input/output is active. */
    bool isActive = false;

    /**%apidoc Timestamp in milliseconds since epoch of last activity on the port. */
    std::chrono::milliseconds timestampMs{};

    bool operator==(const IoStateResponse&) const = default;
};
#define IoStateResponse_Fields IoPortData_Fields(isActive)(timestampMs)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(IoStateResponse, (json))
NX_REFLECTION_INSTRUMENT(IoStateResponse, IoStateResponse_Fields)

struct NX_VMS_API IoStateUpdateRequest
{
    /**%apidoc
     * Device id to set input/output state of the port (can be obtained from "id", "physicalId" or
     * "logicalId" field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain
     * cameras).
     */
    QString deviceId;

    /**%apidoc[opt] Port number. If not specified then default Device port is used. */
    QString port;

    /**%apidoc Set this input/output active or not. */
    bool isActive = false;

    /**%apidoc[opt] Time in milliseconds to reset Device action on action update. */
    std::chrono::milliseconds autoResetTimeoutMs{};

    bool operator==(const IoStateUpdateRequest&) const = default;
};
#define IoStateUpdateRequest_Fields (deviceId)(port)(isActive)(autoResetTimeoutMs)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(IoStateUpdateRequest, (json))
NX_REFLECTION_INSTRUMENT(IoStateUpdateRequest, IoStateUpdateRequest_Fields)

} // namespace nx::vms::api
