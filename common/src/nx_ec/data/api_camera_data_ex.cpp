/**********************************************************
* 2 oct 2014
* akolesnikov
***********************************************************/

#include "api_camera_data_ex.h"

#include <utils/common/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiCameraDataEx), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))
} // namespace ec2
