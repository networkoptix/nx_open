// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "key_value_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    KeyValueData, (ubjson)(json)(xml)(sql_record)(csv_record), KeyValueData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
