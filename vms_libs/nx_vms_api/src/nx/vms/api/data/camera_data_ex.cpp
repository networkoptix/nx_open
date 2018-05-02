#include "camera_data_ex.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CameraDataEx),
    (eq)(ubjson)(xml)(json)(csv_record)(sql_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
