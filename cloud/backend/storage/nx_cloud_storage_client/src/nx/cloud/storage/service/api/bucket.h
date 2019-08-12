#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_CLOUD_STORAGE_CLIENT_API Bucket
{
    // The name of the aws bucket
    std::string name;
    // The region of the bucket, e.g., "us-east-1"
    std::string region;
    // The number of cloud storages associated with this bucket
    int cloudStorageCount = 0;

    bool operator==(const Bucket& other) const;
};

#define Bucket_Fields (name)(region)(cloudStorageCount)

QN_FUSION_DECLARE_FUNCTIONS(Bucket, (ubjson)(json), NX_CLOUD_STORAGE_CLIENT_API)

//-------------------------------------------------------------------------------------------------
// Buckets

/** NOTE: this type exists because std::vector<Bucket> can't be serialized directly */
struct NX_CLOUD_STORAGE_CLIENT_API Buckets
{
    std::vector<Bucket> buckets;
};

#define Buckets_Fields (buckets)

QN_FUSION_DECLARE_FUNCTIONS(Buckets, (json), NX_CLOUD_STORAGE_CLIENT_API)

} // namespace nx::cloud::storage::service::api