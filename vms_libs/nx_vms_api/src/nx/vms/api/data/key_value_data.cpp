#include "key_value_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (KeyValueData),
    (eq)(ubjson)(json)(xml)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
