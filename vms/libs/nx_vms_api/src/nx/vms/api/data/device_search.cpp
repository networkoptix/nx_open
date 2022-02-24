// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_search.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearchStatus, (csv_record)(json)(ubjson)(xml), DeviceSearchStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceSearch, (json), DeviceSearch_Fields)

} // namespace nx::vms::api
