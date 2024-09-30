// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAgentId
{
    /**%apidoc[opt]:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid id;
    nx::Uuid engineId;

    const DeviceAgentId& getId() const { return *this; }
    QString toString() const { return NX_FMT("id: %1, engineId: %2", id, engineId); }
    bool operator==(const DeviceAgentId& other) const = default;
};
#define DeviceAgentId_Fields (id)(engineId)
QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentId, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceAgentId, DeviceAgentId_Fields);

struct NX_VMS_API DeviceAgentState: DeviceAgentId
{
    bool isEnabled = false;

    bool operator==(const DeviceAgentState& other) const = default;
};
#define DeviceAgentState_Fields DeviceAgentId_Fields (isEnabled)
QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentState, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceAgentState, DeviceAgentState_Fields);

} // namespace nx::vms::api::analytics
