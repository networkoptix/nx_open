#include "storage_credentials.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (StorageCredentials),
    (json),
    _Fields)

} // namespace nx::cloud::storage::service::api