// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CloudSystemData, (ubjson)(xml)(json)(sql_record)(csv_record), CloudSystemData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
