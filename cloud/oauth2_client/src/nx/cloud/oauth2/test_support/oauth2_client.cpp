// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/network/http/rest/http_rest_client.h>

#include "../api/request_paths.h"
#include "oauth2_client.h"

namespace nx::cloud::oauth2::client::test {

using namespace db::api;

Oauth2ClientMock::Oauth2ClientMock(Oauth2ClientMockManager& manager):
    m_manager(manager)
{
}

void Oauth2ClientMock::issueToken(
    const db::api::IssueTokenRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>
        completionHandler)
{
    Oauth2ClientMockManager::RequstPath path = {
        api::kOauthTokenPath, nx::network::http::Method::post};
    processRequst<IssueTokenRequest, IssueTokenResponse>(
        path, request, std::move(completionHandler));
}

void Oauth2ClientMock::issueAuthorizationCode(
    const db::api::IssueTokenRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
        completionHandler)
{
    Oauth2ClientMockManager::RequstPath path = {
        api::kOauthTokenPath, nx::network::http::Method::post};
    processRequst<IssueTokenRequest, IssueCodeResponse>(
        path, request, std::move(completionHandler));
}

void Oauth2ClientMock::introspectToken(
    const db::api::TokenIntrospectionRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
        completionHandler)
{
    Oauth2ClientMockManager::RequstPath path = {
        api::kOauthIntrospectPath, nx::network::http::Method::post};
    processRequst<TokenIntrospectionRequest, TokenIntrospectionResponse>(
        path, request, std::move(completionHandler));
}

void Oauth2ClientMock::logout(nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler)
{
    Oauth2ClientMockManager::RequstPath path = {
        api::kOauthLogoutPath, nx::network::http::Method::delete_};
    processRequst<void>(path, std::move(completionHandler));
}

void Oauth2ClientMock::getJwtPublicKeys(
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
        completionHandler)
{
    Oauth2ClientMockManager::RequstPath path = {
        api::kOauthJwksPath, nx::network::http::Method::get};
    processRequst<std::vector<nx::network::jwk::Key>>(path, std::move(completionHandler));
}

void Oauth2ClientMock::getJwtPublicKeyByKid(
    const std::string& kid,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)> completionHandler)
{
    Oauth2ClientMockManager::RequstPath path = {
        nx::network::http::rest::substituteParameters(api::kOauthJwkByIdPath, {kid}),
        nx::network::http::Method::get};
    processRequst<std::string, nx::network::jwk::Key>(path, kid, std::move(completionHandler));
}

void Oauth2ClientMock::introspectSession(
    const std::string& session,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)> completionHandler)
{
    Oauth2ClientMockManager::RequstPath path = {
        nx::network::http::rest::substituteParameters(api::kOauthSessionPath, {session}),
        nx::network::http::Method::get};
    processRequst<std::string, api::GetSessionResponse>(
        path, session, std::move(completionHandler));
}

void Oauth2ClientMock::markSessionMfaVerified(
    const std::string& sessionId, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler)
{
    Oauth2ClientMockManager::RequstPath path = {
        nx::network::http::rest::substituteParameters(api::kOauthSessionPath, {sessionId}),
        nx::network::http::Method::post};
    processRequst<std::string, void>(
        path, sessionId, std::move(handler));
}


void Oauth2ClientMock::internalLogout(
    const std::string&, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2ClientMock::internalIssueToken(
    const std::optional<std::string>&,
    const network::SocketAddress&,
    const nx::cloud::db::api::IssueTokenRequest&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2ClientMock::internalClientLogout(
    const std::string&, const std::string&, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2ClientMock::internalGetSession(const std::string&,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::cloud::db::api::AuthSession)>)
{
    throw std::logic_error("not implemented");
}

void Oauth2ClientMockManager::setResponse(const RequstPath& requestPath, const Response& response)
{
    m_responses[requestPath] = response;
}

void Oauth2ClientMockManager::setRequest(const RequstPath& requestPath, const std::string& request)
{
    m_requests[requestPath] = request;
}

int Oauth2ClientMockManager::getNumCalls(const RequstPath& requestPath)
{
    return m_requestsCounter[requestPath];
}

} // namespace nx::cloud::oauth2::client::test
