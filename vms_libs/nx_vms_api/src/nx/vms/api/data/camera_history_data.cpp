#include "camera_history_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ServerFootageData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CameraHistoryItemData)(CameraHistoryData),
    (eq)(ubjson)(xml)(json)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
