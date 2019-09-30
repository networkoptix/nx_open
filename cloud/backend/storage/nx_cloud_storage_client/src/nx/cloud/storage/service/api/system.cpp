#include "system.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (System),
    (ubjson)(json),
    _Fields)

} // namespace nx::cloud::storge::service::api