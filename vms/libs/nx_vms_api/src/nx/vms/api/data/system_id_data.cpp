// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_id_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    SystemIdData, (json)(ubjson)(xml)(csv_record), SystemIdData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
