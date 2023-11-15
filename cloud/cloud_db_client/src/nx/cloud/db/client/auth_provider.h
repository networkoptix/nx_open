// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/auth_provider.h"

namespace nx::cloud::db::client {

class AuthProvider:
    public api::AuthProvider
{
public:
    AuthProvider(AsyncRequestsExecutor* requestsExecutor);

    virtual void getCdbNonce(
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) override;

    virtual void getCdbNonce(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) override;

    virtual void getAuthenticationResponse(
        const api::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler) override;

    virtual void resolveUserCredentials(
        const api::UserAuthorization& authorization,
        std::function<void(api::ResultCode, api::CredentialsDescriptor)> completionHandler) override;

    virtual void resolveUserCredentialsList(
        const api::UserAuthorizationList& authorizationList,
        std::function<void(api::ResultCode, api::CredentialsDescriptorList, std::string)>
            completionHandler) override;

    virtual void getSystemAccessLevel(
        const std::string& systemId,
        const api::UserAuthorization& authorization,
        std::function<void(api::ResultCode, api::SystemAccess)> completionHandler) override;

    virtual void getSystemAccessLevel(
        const std::vector<api::SystemAccessLevelRequest>& requests,
        std::function<void(api::ResultCode, std::vector<api::SystemAccess>)> completionHandler) override;

    virtual void getVmsServerTlsPublicKey(
        const std::string& systemId,
        const std::string& serverId,
        const std::string& fingerprint,
        bool isValid,
        std::function<void(api::ResultCode,
            api::VmsServerCertificatePublicKey,
            std::chrono::system_clock::time_point)> completionHandler) override;

private:
    AsyncRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::client
