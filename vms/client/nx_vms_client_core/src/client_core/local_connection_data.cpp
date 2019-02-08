#include "local_connection_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::core {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LocalConnectionData)(WeightData),
    (eq)(json), _Fields)

} // namespace nx::vms::client::core
