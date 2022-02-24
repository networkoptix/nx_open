// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_type_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PropertyTypeData, (ubjson)(xml)(json)(sql_record)(csv_record), PropertyTypeData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ResourceTypeData, (ubjson)(xml)(json)(sql_record)(csv_record), ResourceTypeData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
