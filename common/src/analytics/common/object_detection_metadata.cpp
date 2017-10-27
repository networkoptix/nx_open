#include "object_detection_metadata.h"

#include <tuple>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace common {
namespace metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_OBJECT_DETECTION_TYPES,
    (json)(ubjson),
    _Fields,
    (brief, true))

} // namespace metadata
} // namespace common
} // namespace nx
