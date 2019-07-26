#include "add_bucket.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (AddBucketRequest),
    (json),
    _Fields)

} // namespace nx::cloud::storage::service::api