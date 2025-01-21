// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_runtime_event_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ServerRuntimeEventData,
    (json)(ubjson),
    nx_vms_api_ServerRuntimeEventData_Fields,
    (optional, true))

} // namespace nx::vms::api
