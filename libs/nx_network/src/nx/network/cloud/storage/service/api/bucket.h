#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API Bucket
{
    // The name of the aws bucket;
    std::string name;
    // The region of the bucket, e.g., "us-east-1"
    std::string region;
    // The number of cloud storages associated with this bucket
    int cloudStorageCount;
};

#define Bucket_Fields (name)(region)(cloudStorageCount)

QN_FUSION_DECLARE_FUNCTIONS(Bucket, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api