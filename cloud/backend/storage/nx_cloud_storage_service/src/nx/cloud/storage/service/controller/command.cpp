#include "command.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::controller {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Bucket),
    (json)(ubjson),
    _sync_Fields)

} // namespace nx::cloud::storage::service::controller