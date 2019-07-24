#include "add_storage.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AddStorageRequest),
    (json),
    _Fields)

} // namespace nx::cloud::storage::api