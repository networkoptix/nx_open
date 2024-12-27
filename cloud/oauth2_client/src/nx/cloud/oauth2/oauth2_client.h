// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/cloud/db/api/result_code.h>
#include <nx/cloud/db/client/async_http_requests_executor.h>
#include <nx/network/http/generic_api_client.h>
#include <nx/utils/basic_factory.h>

#include "api/data.h"

namespace nx::cloud::utils {

class AbstractJwtFetcher;

} // namespace nx::cloud::utils

namespace nx::cloud::oauth2::client {

/**
 * Client for Oauth2 Service API.
 * For details see Oauth2 service documentation
 */
class AbstractOauth2Client
{
public:
    virtual ~AbstractOauth2Client() = default;

    virtual void issueToken(
        const db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>
            completionHandler) = 0;

    virtual void issueAuthorizationCode(
        const db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) = 0;

    virtual void introspectToken(
        const db::api::TokenIntrospectionRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
            completionHandler) = 0;

    virtual void logout(nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) = 0;

    virtual void getJwtPublicKeys(
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
            completionHandler) = 0;

    virtual void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)>
            completionHandler) = 0;

    virtual void introspectSession(
        const std::string& session,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)>
            completionHandler) = 0;

    virtual void markSessionMfaVerified(
        const std::string& sessionId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) = 0;

    // TODO: #anekrasov Do we need this?
    virtual void internalClientLogout(
        const std::string& email,
        const std::string& clienId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) = 0;

    // TODO: #anekrasov Do we need this?
    virtual void internalLogout(
        const std::string& email, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) = 0;

    // TODO: #anekrasov Do we need this?
    virtual void internalIssueToken(
        const std::optional<std::string>& hostName,
        const network::SocketAddress& clientEndpoint,
        const nx::cloud::db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)> handler) = 0;

    // TODO: #anekrasov Do we need this?
    virtual void internalGetSession(
        const std::string& sessionId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::cloud::db::api::AuthSession)>
            handler) = 0;
};

class Oauth2Client:
    public AbstractOauth2Client,
    public nx::network::http::GenericApiClient<db::client::ResultCodeDescriptor>
{
    using base_type = nx::network::http::GenericApiClient<db::client::ResultCodeDescriptor>;

public:
    Oauth2Client(
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials);

    void issueToken(
        const db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>
            completionHandler) override;

    void issueAuthorizationCode(
        const db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) override;

    void introspectToken(
        const db::api::TokenIntrospectionRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
            completionHandler) override;

    void logout(nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

    void getJwtPublicKeys(
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
            completionHandler) override;

    void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)>
            completionHandler) override;

    void introspectSession(
        const std::string& session,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)>
            completionHandler) override;

    void internalClientLogout(
        const std::string& email,
        const std::string& clienId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

    void internalLogout(
        const std::string& email, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

    void internalIssueToken(
        const std::optional<std::string>& hostName,
        const network::SocketAddress& clientEndpoint,
        const nx::cloud::db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)> handler) override;

    void internalGetSession(
        const std::string& sessionId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::cloud::db::api::AuthSession)>
            handler) override;

    void markSessionMfaVerified(
        const std::string& sessionId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;
};

using Oauth2ClientFactoryFunc = std::unique_ptr<AbstractOauth2Client>(
    const nx::utils::Url&, const nx::network::http::Credentials&);

class Oauth2ClientFactory: public nx::utils::BasicFactory<Oauth2ClientFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<Oauth2ClientFactoryFunc>;

public:
    Oauth2ClientFactory();

    static Oauth2ClientFactory& instance();
};

} // namespace nx::cloud::oauth2::client
