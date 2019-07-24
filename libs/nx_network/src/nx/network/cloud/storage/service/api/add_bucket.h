#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::storage::service::api {

struct NX_NETWORK_API AddBucketRequest
{
    // The name of the bucket to add to cloud storage
    std::string name;
};

#define AddBucketRequest_Fields (name)

QN_FUSION_DECLARE_FUNCTIONS(AddBucketRequest, (json), NX_NETWORK_API)

//-------------------------------------------------------------------------------------------------
// Error

/**
 * Used to forward the the error from actual service that generated it, if one occured
 */
struct NX_NETWORK_API Error
{
    std::string resultCode;
    std::string text;
};

#define Error_Fields (resultCode)(text)

QN_FUSION_DECLARE_FUNCTIONS(Error, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api