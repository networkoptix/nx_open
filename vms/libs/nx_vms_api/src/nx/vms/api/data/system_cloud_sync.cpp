// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_cloud_sync.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemCloudSyncRequest, (json), SystemCloudSyncRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CloudPullingStatus, (json), CloudPullingStatus_Fields)

} // namespace nx::vms::api
