// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "id_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    IdData, (ubjson)(xml)(json)(sql_record)(csv_record), IdData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
