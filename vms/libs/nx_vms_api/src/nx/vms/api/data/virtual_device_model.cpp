// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_device_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(FlexibleId, (json), FlexibleId_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceCreate, (json), VirtualDeviceCreate_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceLockInfo, (json), VirtualDeviceLockInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceStatus, (json), VirtualDeviceStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceFootageElement, (json), VirtualDeviceFootageElement_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDevicePrepare, (json), VirtualDevicePrepare_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDevicePrepareReply, (json), VirtualDevicePrepareReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceConsume, (json), VirtualDeviceConsume_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceLock, (json), VirtualDeviceLock_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceExtend, (json), VirtualDeviceExtend_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceRelease, (json), VirtualDeviceRelease_Fields)

} // namespace nx::vms::api
