// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAgentState
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QnUuid id;
    QnUuid engineId;
    bool isEnabled = false;

    DeviceAgentState() = default;
    DeviceAgentState(const QnUuid& id): id(id) {}
    bool operator==(const DeviceAgentState& other) const = default;

    QnUuid getId() const { return id; }
};
#define DeviceAgentState_Fields (id)(engineId)(isEnabled)
QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentState, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceAgentState, DeviceAgentState_Fields);

} // namespace nx::vms::api::analytics
