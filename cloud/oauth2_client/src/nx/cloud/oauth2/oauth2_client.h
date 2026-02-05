// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/result_code.h>
#include <nx/cloud/db/client/async_http_requests_executor.h>
#include <nx/cloud/db/client/data/oauth_data.h>
#include <nx/network/http/generic_api_client.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "api/data.h"

namespace nx::cloud::utils {

class AbstractJwtFetcher;

} // namespace nx::cloud::utils

namespace nx::cloud::oauth2::client {

/**
 * Client for Oauth2 Service API.
 * For details see Oauth2 service documentation
 */
class NX_OAUTH2_CLIENT_API AbstractOauth2Client
{
public:
    virtual ~AbstractOauth2Client() = default;

    virtual void issueToken(
        const db::api::IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>
            completionHandler) = 0;

    virtual void issueAuthorizationCode(
        const db::api::IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) = 0;

    virtual void issuePasswordResetCode(
        const db::api::IssuePasswordResetCodeRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) = 0;

    virtual void introspectToken(
        const db::api::TokenIntrospectionRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
            completionHandler) = 0;

    virtual void logout(nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) = 0;

    virtual void clientLogout(
        const std::string& clientId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) = 0;

    virtual void getJwtPublicKeys(
        nx::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
            completionHandler) = 0;

    virtual void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)>
            completionHandler) = 0;

    virtual void introspectSession(
        const std::string& session,
        nx::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)>
            completionHandler) = 0;

    virtual void markSessionMfaVerified(
        const std::string& sessionId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> handler) = 0;

    virtual void notifyAccountUpdated(
        const db::api::AccountChangedEvent& event,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) = 0;

    virtual void issueServiceToken(const api::IssueServiceTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, api::IssueServiceTokenResponse)>
            completionHandler) = 0;

    virtual void setCredentials(network::http::Credentials credentials) = 0;

    // It shouldn't be here.
    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) = 0;

    virtual network::http::ClientOptions& httpClientOptions() = 0;
};

class NX_OAUTH2_CLIENT_API Oauth2Client:
    public AbstractOauth2Client,
    public nx::cloud::db::client::ApiRequestsExecutor
{
    using base_type = nx::cloud::db::client::ApiRequestsExecutor;

public:
    Oauth2Client(
        const nx::Url& url,
        const std::optional<nx::network::http::Credentials>& credentials,
        int idleConnectionsLimit);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    void issueToken(
        const db::api::IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>
            completionHandler) override;

    void issueAuthorizationCode(
        const db::api::IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) override;

    virtual void issuePasswordResetCode(
        const db::api::IssuePasswordResetCodeRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) override;

    void introspectToken(
        const db::api::TokenIntrospectionRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
            completionHandler) override;

    void logout(nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

    void clientLogout(
        const std::string& clientId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

    void getJwtPublicKeys(
        nx::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
            completionHandler) override;

    void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)>
            completionHandler) override;

    void introspectSession(
        const std::string& session,
        nx::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)>
            completionHandler) override;

    void markSessionMfaVerified(
        const std::string& sessionId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

    void notifyAccountUpdated(
        const db::api::AccountChangedEvent& event,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

    virtual void issueServiceToken(
        const api::IssueServiceTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, api::IssueServiceTokenResponse)>
            completionHandler) override;

    void setCredentials(network::http::Credentials credentials) override;

    nx::network::http::ClientOptions& httpClientOptions() override;
};

using Oauth2ClientFactoryFunc = std::unique_ptr<AbstractOauth2Client>(
    const nx::Url&, const std::optional<nx::network::http::Credentials>&, int);

class NX_OAUTH2_CLIENT_API Oauth2ClientFactory:
    public nx::utils::BasicFactory<Oauth2ClientFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<Oauth2ClientFactoryFunc>;

public:
    Oauth2ClientFactory();

    static Oauth2ClientFactory& instance();
};

} // namespace nx::cloud::oauth2::client
