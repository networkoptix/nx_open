// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_space_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageSpaceData, (csv_record)(json)(ubjson)(xml), StorageSpaceData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StorageSpaceDataWithDbInfo,
    (csv_record)(json)(ubjson)(xml), StorageSpaceDataWithDbInfo_Fields)

} // namespace nx::vms::api
