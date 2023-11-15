// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/oauth_manager.h"

namespace nx::cloud::db::client {

class OauthManager:
    public api::OauthManager
{
public:
    OauthManager(AsyncRequestsExecutor* requestsExecutor);

    virtual void issueToken(
        const api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueTokenResponse)> completionHandler)
        override;

    virtual void issueAuthorizationCode(
        const api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueCodeResponse)> completionHandler)
        override;

    virtual void validateToken(
        const std::string& token,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::ValidateTokenResponse)>
            completionHandler)
        override;

    virtual void deleteToken(
        const std::string& token,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void deleteTokens(
        const std::string& clientId,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void logout(nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    void issueStunToken(
        const api::IssueStunTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueStunTokenResponse)>
            completionHandler) override;

private:
    AsyncRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::client
