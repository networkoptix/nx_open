// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_data_ex.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraDataEx, (ubjson)(xml)(json)(csv_record)(sql_record), CameraDataEx_Fields)


} // namespace api
} // namespace vms
} // namespace nx

