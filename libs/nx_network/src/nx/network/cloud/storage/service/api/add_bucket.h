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
// BadRequest

/**
 * A structure that is only valid if HTTP status code is not 200 OK
 */
struct AddBucketResponse
{
    std::string resultCode;
    std::string errorText;
};

#define AddBucketResponse_Fields (resultCode)(errorText)

QN_FUSION_DECLARE_FUNCTIONS(AddBucketResponse, (json), NX_NETWORK_API)

} // namespace nx::cloud::storage::service::api