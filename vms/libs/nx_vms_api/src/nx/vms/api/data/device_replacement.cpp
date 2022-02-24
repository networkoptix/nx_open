// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_replacement.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceReplacementRequest,
    (json),
    DeviceReplacementRequest_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceReplacementResponse,
    (json),
    DeviceReplacementResponse_Fields)

} // namespace nx::vms::api