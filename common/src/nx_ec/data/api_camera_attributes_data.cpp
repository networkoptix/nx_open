/**********************************************************
* 2 oct 2014
* akolesnikov
***********************************************************/

#include "api_camera_attributes_data.h"

#include "api_model_functions_impl.h"


namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiScheduleTaskData) (ApiScheduleTaskWithRefData) (ApiCameraAttributesData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)
} // namespace ec2
