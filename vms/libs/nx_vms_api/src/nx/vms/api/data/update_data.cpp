// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UpdateUploadData, (ubjson)(xml)(json)(sql_record)(csv_record), UpdateUploadData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateUploadResponseData,
    (ubjson)(xml)(json)(sql_record)(csv_record), UpdateUploadResponseData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UpdateInstallData, (ubjson)(xml)(json)(sql_record)(csv_record), UpdateInstallData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
