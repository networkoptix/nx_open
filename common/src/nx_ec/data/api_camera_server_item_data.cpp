#include "api_camera_server_item_data.h"
#include "api_model_functions_impl.h"

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiCameraHistoryData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiCameraHistoryMoveData), (ubjson)(xml)(json)(csv_record), _Fields)
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiCameraHistoryDetailData), (ubjson)(xml)(json)(csv_record), _Fields)
} // namespace ec2
