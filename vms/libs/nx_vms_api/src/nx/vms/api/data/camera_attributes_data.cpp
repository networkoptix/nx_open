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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ScheduleTaskData)(ScheduleTaskWithRefData)(CameraAttributesData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
