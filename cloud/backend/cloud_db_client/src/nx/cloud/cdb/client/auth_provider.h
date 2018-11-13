#pragma once

#include "async_http_requests_executor.h"
#include "include/nx/cloud/cdb/api/auth_provider.h"

namespace nx::cdb::client {

class AuthProvider:
    public api::AuthProvider,
    public AsyncRequestsExecutor
{
public:
    AuthProvider(network::cloud::CloudModuleUrlFetcher* const cloudModuleEndPointFetcher);

    virtual void getCdbNonce(
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) override;
    virtual void getCdbNonce(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) override;
    virtual void getAuthenticationResponse(
        const api::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler) override;
};


} // namespace nx::cdb::client
