// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stored_file_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StoredFileData, (ubjson)(xml)(json)(sql_record)(csv_record), StoredFileData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StoredFilePath, (ubjson)(xml)(json)(sql_record)(csv_record), StoredFilePath_Fields)

} // namespace api
} // namespace vms
} // namespace nx
