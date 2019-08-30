#pragma once

#include "nx/cloud/aws/base_api_client.h"

#include "assume_role_request.h"

namespace nx::cloud::aws::sts {

class NX_AWS_CLIENT_API ApiClient:
    public BaseApiClient
{
    using base_type = BaseApiClient;

public:
    ApiClient(
        const std::string& awsRegion,
        const nx::utils::Url& url,
        const aws::Credentials& credentials);
    ~ApiClient();

    void assumeRole(
        const AssumeRoleRequest& request,
        nx::utils::MoveOnlyFunc<void(Result, AssumeRoleResult)> handler);
};

} // namespace nx::cloud::aws::sts
