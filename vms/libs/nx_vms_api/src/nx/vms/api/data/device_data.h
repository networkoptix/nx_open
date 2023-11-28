// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API ConnectedDevicesRequest
{
    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be "this" to
     * refer to the current Server.
     * %example this
     */
    QnUuid serverId;
    std::vector<QString> deviceIds;
};
#define ConnectedDevicesRequest_Fields (serverId)(deviceIds)
NX_VMS_API_DECLARE_STRUCT_EX(ConnectedDevicesRequest, (json))
NX_REFLECTION_INSTRUMENT(ConnectedDevicesRequest, ConnectedDevicesRequest_Fields)

} // namespace nx::vms::api
