#include "connection_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ConnectionData),
    (ubjson)(json),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
