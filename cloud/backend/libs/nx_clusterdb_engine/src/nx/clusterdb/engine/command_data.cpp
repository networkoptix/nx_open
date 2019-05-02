#include "command_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::clusterdb::engine {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CommandData),
    (json)(ubjson),
    _Fields,
    (optional, true))

} // namespace nx::clusterdb::engine
