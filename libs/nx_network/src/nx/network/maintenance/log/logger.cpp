#include "logger.h"

#include <nx/fusion/model_functions.h>

namespace nx::network::maintenance::log {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Filter)(Logger)(Loggers),
    (json),
    _Fields)

} // namespace nx::network::maintenance::log
