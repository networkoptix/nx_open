#pragma once

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/auth_provider.h"

namespace nx::cloud::db::client {

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


} // namespace nx::cloud::db::client
