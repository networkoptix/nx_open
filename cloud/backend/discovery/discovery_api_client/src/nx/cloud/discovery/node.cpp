#include "node.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::discovery {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (NodeInfo)(Node),
    (json),
    _Fields)

} // namespace nx::cloud::discovery
