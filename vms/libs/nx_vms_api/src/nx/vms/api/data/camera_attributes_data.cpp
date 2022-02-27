// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_attributes_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

DeprecatedFieldNames* CameraAttributesData::getDeprecatedFieldNames()
{
    static DeprecatedFieldNames kDeprecatedFieldNames{
        {"cameraId", "cameraID"},                  //< up to v2.6
        {"preferredServerId", "preferedServerId"}, //< up to v2.6
    };
    return &kDeprecatedFieldNames;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ScheduleTaskData, (ubjson)(xml)(json)(sql_record)(csv_record), ScheduleTaskData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraAttributesData,
    (ubjson)(xml)(json)(sql_record)(csv_record), CameraAttributesData_Fields)


} // namespace api
} // namespace vms
} // namespace nx
