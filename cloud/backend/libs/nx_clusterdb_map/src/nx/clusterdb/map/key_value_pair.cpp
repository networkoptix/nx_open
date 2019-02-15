#include "key_value_pair.h"

#include <nx/fusion/model_functions.h>

namespace nx::clusterdb::map {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
(Key)(KeyValuePair),
(ubjson)(json),
_Fields)

} // namespace nx::clusterdb::map
