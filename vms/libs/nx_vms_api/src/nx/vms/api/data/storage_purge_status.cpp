// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_purge_status.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StoragePurgeStatusData::LastPurgeData, (json), LastPurgeData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StoragePurgeStatusData, (json), StoragePurgeStatusData_Fields)

} // namespace nx::vms::api
