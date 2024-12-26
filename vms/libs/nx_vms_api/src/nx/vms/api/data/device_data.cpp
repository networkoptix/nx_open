// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ConnectedDevicesRequest, (json), ConnectedDevicesRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceIoFilter, (json), DeviceIoFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IoPortState, (json), IoPortState_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceIoState, (json), DeviceIoState_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IoPortUpdateRequest, (json), IoPortUpdateRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceIoUpdateRequest, (json), DeviceIoUpdateRequest_Fields)

} // namespace nx::vms::api
