// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent_state.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAgentId, (json), DeviceAgentId_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceAgentState, (json), DeviceAgentState_Fields)

} // namespace nx::vms::api::analytics
