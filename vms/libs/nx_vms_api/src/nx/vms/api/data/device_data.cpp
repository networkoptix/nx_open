// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ConnectedDevicesRequest, (json), ConnectedDevicesRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IoPortData, (json), IoPortData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IoStateResponse, (json), IoStateResponse_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IoStateUpdateRequest, (json), IoStateUpdateRequest_Fields)

} // namespace nx::vms::api
