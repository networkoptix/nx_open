// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/jose/jwk.h>

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/oauth_manager.h"

namespace nx::cloud::db::client {

class OauthManager:
    public api::OauthManager
{
public:
    OauthManager(ApiRequestsExecutor* requestsExecutor);

    virtual std::chrono::seconds lastServerTime() const override;

    virtual void issueToken(
        const api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueTokenResponse)> completionHandler)
        override;

    virtual void issueAuthorizationCode(
        const api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueCodeResponse)> completionHandler)
        override;

    virtual void legacyValidateToken(
        const std::string& token,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::ValidateTokenResponse)>
            completionHandler)
        override;

    virtual void introspectToken(
        const api::TokenIntrospectionRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::TokenIntrospectionResponse)>
            completionHandler) override;

    virtual void deleteToken(
        const std::string& token,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void deleteTokensByClientId(
        const std::string& clientId,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void logout(nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    void issueStunToken(
        const api::IssueStunTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueStunTokenResponse)>
            completionHandler) override;

    virtual void getJwtPublicKeys(
        nx::utils::MoveOnlyFunc<void(api::ResultCode, std::vector<nx::network::jwk::Key>)>
            completionHandler) override;

    virtual void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, nx::network::jwk::Key)>
            completionHandler) override;

    virtual void getAccSecuritySettingsChangedEvents(
        const api::GetAccSecuritySettingsChangedEventsRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode,
            api::GetAccSecuritySettingsChangedEventsResponse)> completionHandler) override;

private:
    ApiRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::client
