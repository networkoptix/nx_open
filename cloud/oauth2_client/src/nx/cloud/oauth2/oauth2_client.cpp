// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth2_client.h"

namespace nx::cloud::oauth2::client {

Oauth2Client::Oauth2Client(
    const nx::utils::Url&,
    const nx::network::http::Credentials&)
{
}

void Oauth2Client::issueToken(
    const db::api::IssueTokenRequest&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::issueAuthorizationCode(
    const db::api::IssueTokenRequest&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::introspectToken(
    const db::api::TokenIntrospectionRequest&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::logout(nx::utils::MoveOnlyFunc<void(db::api::ResultCode)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::getJwtPublicKeys(
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::getJwtPublicKeyByKid(
    const std::string&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2Client::introspectSession(
    const std::string&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)>)
{
    throw std::logic_error("not implemented");
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
