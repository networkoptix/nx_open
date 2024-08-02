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

struct NX_VMS_API IoPortData: IdData
{
    /**%apidoc Port number. */
    QString port;
};
#define IoPortData_Fields IdData_Fields (port)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(IoPortData, (json))

struct NX_VMS_API IoStateData: IoPortData
{
    /**%apidoc Indicates if this input/output is active. */
    bool isActive = false;

    /**%apidoc Timestamp in milliseconds since epoch of last activity on the port. */
    std::optional<std::chrono::milliseconds> timestampMs = std::nullopt;

    /**%apidoc Time in milliseconds to reset Device action on action update. */
    std::optional<std::chrono::milliseconds> autoResetTimeoutMs = std::nullopt;

    bool operator==(const IoStateData& other) const = default;
};
#define IoStateData_Fields IoPortData_Fields (isActive)(timestampMs)(autoResetTimeoutMs)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(IoStateData, (json))
NX_REFLECTION_INSTRUMENT(IoStateData, IoStateData_Fields)

} // namespace nx::vms::api
