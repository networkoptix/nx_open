#include "system_id_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemIdData),
    (eq)(json)(ubjson)(xml)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
