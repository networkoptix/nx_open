// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_history_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ServerFootageData, (ubjson)(xml)(json)(sql_record)(csv_record), ServerFootageData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraHistoryItemData, (ubjson)(xml)(json)(csv_record), CameraHistoryItemData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraHistoryData, (ubjson)(xml)(json)(csv_record), CameraHistoryData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
