// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth2_client.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "api/request_paths.h"

namespace nx::cloud::oauth2::client {

Oauth2Client::Oauth2Client(
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials):
    base_type(url, nx::network::ssl::kDefaultCertificateCheck)
{
    setHttpCredentials(credentials);
}

void Oauth2Client::issueToken(
    const db::api::IssueTokenRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)> handler)
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
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)> handler)
{
    base_type::template makeAsyncCall<db::api::IssueCodeResponse>(
        nx::network::http::Method::post,
        api::kOauthTokenPath,
        {}, // query
        std::move(request),
        std::move(handler));
}

void Oauth2Client::introspectToken(
    const db::api::TokenIntrospectionRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)> handler)
{
    base_type::template makeAsyncCall<db::api::TokenIntrospectionResponse>(
        nx::network::http::Method::post,
        api::kOauthIntrospectPath,
        {}, // query
        std::move(request),
        std::move(handler));
}

void Oauth2Client::logout(nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::delete_,
        api::kOauthIntrospectPath,
        {}, // query
        std::move(handler));
}

void Oauth2Client::getJwtPublicKeys(
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)> handler)
{
    base_type::template makeAsyncCall<std::vector<nx::network::jwk::Key>>(
        nx::network::http::Method::get,
        api::kOauthJwksPath,
        {}, // query
        std::move(handler));
}

void Oauth2Client::getJwtPublicKeyByKid(
    const std::string& kid,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)> handler)
{
    base_type::template makeAsyncCall<nx::network::jwk::Key>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(api::kOauthJwkByIdPath, {kid}),
        {}, // query
        std::move(handler));
}

void Oauth2Client::introspectSession(
    const std::string& session,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)> handler)
{
    base_type::template makeAsyncCall<api::GetSessionResponse>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(api::kOauthSessionPath, {session}),
        {}, // query
        std::move(handler));
}

void Oauth2Client::internalLogout(
    const std::string&, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::internalIssueToken(
    const std::optional<std::string>&,
    const network::SocketAddress&,
    const nx::cloud::db::api::IssueTokenRequest&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::internalClientLogout(
    const std::string&, const std::string&, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::internalGetSession(
    const std::string&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::cloud::db::api::AuthSession)>)
{
    throw std::logic_error("not implemented");
}

Oauth2ClientFactory::Oauth2ClientFactory():
    base_type([](const nx::utils::Url& url, const nx::network::http::Credentials& credentials)
        { return std::make_unique<Oauth2Client>(url, credentials); })
{
}

Oauth2ClientFactory& Oauth2ClientFactory::instance()
{
    static Oauth2ClientFactory staticInstance;
    return staticInstance;
}

} // namespace nx::cloud::oauth2::client
