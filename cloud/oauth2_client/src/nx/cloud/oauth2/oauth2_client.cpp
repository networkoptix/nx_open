// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth2_client.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/utils/log/log_main.h>

#include "api/request_paths.h"

namespace nx::cloud::oauth2::client {

static constexpr auto kDefaultRequestTimeout = std::chrono::seconds(10);

Oauth2Client::Oauth2Client(
    const nx::Url& url,
    const std::optional<nx::network::http::Credentials>& credentials,
    int idleConnectionsLimit):
    base_type(url, nx::network::ssl::kDefaultCertificateCheck, idleConnectionsLimit)
{
    setRequestTimeout(kDefaultRequestTimeout);
    if (credentials)
        setHttpCredentials(*credentials);
}

void Oauth2Client::issueToken(
    const db::api::IssueTokenRequest& request,
    nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)> handler)
{
    base_type::template makeAsyncCall<db::api::IssueTokenResponse>(
        nx::network::http::Method::post,
        api::kOauthTokenPath,
        {}, // query
        std::move(request),
        std::move(handler));
}

void Oauth2Client::issueAuthorizationCode(
    const db::api::IssueTokenRequest& request,
    nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)> handler)
{
    base_type::template makeAsyncCall<db::api::IssueCodeResponse>(
        nx::network::http::Method::post,
        api::kOauthTokenPath,
        {}, // query
        std::move(request),
        std::move(handler));
}

void Oauth2Client::issuePasswordResetCode(
    const db::api::IssuePasswordResetCodeRequest& request,
    nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)> handler)
{
    base_type::template makeAsyncCall<db::api::IssueCodeResponse>(
        nx::network::http::Method::post,
        api::kOauthPasswordResetCodePath,
        {}, // query
        std::move(request),
        std::move(handler));
}

void Oauth2Client::introspectToken(
    const db::api::TokenIntrospectionRequest& request,
    nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)> handler)
{
    base_type::template makeAsyncCall<db::api::TokenIntrospectionResponse>(
        nx::network::http::Method::post,
        api::kOauthIntrospectPath,
        {}, // query
        std::move(request),
        std::move(handler));
}

void Oauth2Client::logout(nx::MoveOnlyFunc<void(db::api::ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::delete_,
        api::kOauthLogoutPath,
        {}, // query
        std::move(handler));
}

void Oauth2Client::clientLogout(
    const std::string& clientId,
    nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(api::kOauthClientLogoutPath, {clientId}),
        {}, // query
        std::move(completionHandler));
}

void Oauth2Client::getJwtPublicKeys(
    nx::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)> handler)
{
    base_type::template makeAsyncCall<std::vector<nx::network::jwk::Key>>(
        nx::network::http::Method::get,
        api::kOauthJwksPath,
        {}, // query
        std::move(handler));
}

void Oauth2Client::getJwtPublicKeyByKid(
    const std::string& kid,
    nx::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)> handler)
{
    base_type::template makeAsyncCall<nx::network::jwk::Key>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(api::kOauthJwkByIdPath, {kid}),
        {}, // query
        std::move(handler));
}

void Oauth2Client::introspectSession(
    const std::string& session,
    nx::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)> handler)
{
    base_type::template makeAsyncCall<api::GetSessionResponse>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(api::kOauthSessionPath, {session}),
        {}, // query
        std::move(handler));
}

void Oauth2Client::markSessionMfaVerified(
    const std::string& sessionId, nx::MoveOnlyFunc<void(db::api::ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(api::kOauthSessionPath, {sessionId}),
        {}, // query
        std::move(handler));
}

void Oauth2Client::notifyAccountUpdated(
    const db::api::AccountChangedEvent& event,
    nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::post,
        api::kAccountChangedEventHandler,
        {}, // query
        std::move(event),
        std::move(completionHandler));
}

void Oauth2Client::issueServiceToken(
    const api::IssueServiceTokenRequest& request,
    nx::MoveOnlyFunc<void(db::api::ResultCode, api::IssueServiceTokenResponse)> completionHandler)
{
    base_type::template makeAsyncCall<api::IssueServiceTokenResponse>(
        nx::network::http::Method::post,
        api::kOauthServiceToken,
        {}, // query
        std::move(request),
        std::move(completionHandler));
}

void Oauth2Client::setCredentials(network::http::Credentials credentials)
{
    httpClientOptions().setCredentials(credentials);
}

void Oauth2Client::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
}

Oauth2ClientFactory::Oauth2ClientFactory():
    base_type([](
        const nx::Url& url,
        const std::optional<nx::network::http::Credentials>& credentials,
        int idleConnectionsLimit)
        { return std::make_unique<Oauth2Client>(url, credentials, idleConnectionsLimit); })
{
}

Oauth2ClientFactory& Oauth2ClientFactory::instance()
{
    static Oauth2ClientFactory staticInstance;
    return staticInstance;
}

} // namespace nx::cloud::oauth2::client
