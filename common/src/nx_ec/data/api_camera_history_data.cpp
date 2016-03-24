#include "api_camera_history_data.h"

#include <utils/common/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
        (ApiServerFootageData),
        (ubjson)(xml)(json)(csv_record)(sql_record), _Fields, (optional, true))
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
        (ApiCameraHistoryItemData)(ApiCameraHistoryData),
        (ubjson)(xml)(json)(csv_record), _Fields, (optional, true))

} // namespace ec2
