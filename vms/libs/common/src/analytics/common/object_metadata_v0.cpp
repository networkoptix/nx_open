#include "object_metadata_v0.h"

#include <nx/fusion/model_functions.h>

namespace nx::common::metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ObjectMetadataPacketV0)(ObjectMetadataV0),
    (json)(ubjson),
    _Fields,
    (brief, true))

} // namespace nx::common::metadata
