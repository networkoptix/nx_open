// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_device_model.h"

#include <nx/fusion/model_functions.h>

// -- TODO: #skolesnik Test and remove if not working --------
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

namespace nx::vms::api {

// -- Create Uploads Request --------
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VideoFileMetaInfo, (json), VideoFileMetaInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceArchiveUploadItem, (json), VirtualDeviceFileInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceCreateUploads, (json), VirtualDeviceCreateUploads_Fields)

// -- Create Uploads Response --------
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResultForVirtualDeviceCreateUpload, (json), ResultForVirtualDeviceCreateUpload_Fields)

// -- Write Upload Request --------
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VirtualDeviceUploadId, (json), VirtualDeviceUploadId_Fields)

// -- Write Upload \ Read Status Response --------
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StatusForVirtualDeviceUpload, (json), StatusForVirtualDeviceUpload_Fields)

} // namespace nx::vms::api
