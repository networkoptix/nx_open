#pragma once

#include <nx/network/http/test_http_server.h>

#include "nx/cloud/aws/sts/assume_role_request.h"

namespace nx::cloud::aws::sts::test {

class NX_AWS_CLIENT_API AwsStsEmulator
{
public:
    AwsStsEmulator();

    bool bindAndListen();

    nx::utils::Url url() const;

    std::optional<AssumeRoleResult> getAssumedRole(const std::string& accessKeyId);

private:
    void registerHttpApi();

    void assumeRole(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

private:
    nx::network::http::TestHttpServer m_server;
    mutable QnMutex m_mutex;
    std::map<std::string/*accessKeyId*/, AssumeRoleResult> m_assumedRoles;
};

} // namespace nx::cloud::aws::sts::test