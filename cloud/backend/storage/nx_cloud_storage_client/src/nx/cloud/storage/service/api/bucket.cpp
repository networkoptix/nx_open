#include "bucket.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Bucket),
    (ubjson)(json),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Buckets),
    (json),
    _Fields)

bool Bucket::operator==(const Bucket& other) const
{
    return name == other.name
        && region == other.region
        && cloudStorageCount == other.cloudStorageCount;
}

} // namespace nx::cloud::storage::service::api