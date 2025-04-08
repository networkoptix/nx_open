// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth2_client_mock.h"

#include <utility>

#include <nx/network/http/rest/http_rest_client.h>

#include "../api/request_paths.h"

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
    Oauth2ClientMockManager::RequestPath path = {
        api::kOauthTokenPath, nx::network::http::Method::post};
    processRequest<IssueTokenRequest, IssueTokenResponse>(
        path, request, std::move(completionHandler));
}

void Oauth2ClientMock::issueAuthorizationCode(
    const db::api::IssueTokenRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
        completionHandler)
{
    Oauth2ClientMockManager::RequestPath path = {
        api::kOauthTokenPath, nx::network::http::Method::post};
    processRequest<IssueTokenRequest, IssueCodeResponse>(
        path, request, std::move(completionHandler));
}

void Oauth2ClientMock::issuePasswordResetCode(
    const db::api::IssuePasswordResetCodeRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
        completionHandler)
{
    Oauth2ClientMockManager::RequestPath path = {
        api::kOauthPasswordResetCodePath, nx::network::http::Method::post};
    processRequest<IssuePasswordResetCodeRequest, IssueCodeResponse>(
        path, request, std::move(completionHandler));
}

void Oauth2ClientMock::introspectToken(
    const db::api::TokenIntrospectionRequest& request,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
        completionHandler)
{
    Oauth2ClientMockManager::RequestPath path = {
        api::kOauthIntrospectPath, nx::network::http::Method::post};
    processRequest<TokenIntrospectionRequest, TokenIntrospectionResponse>(
        path, request, std::move(completionHandler));
}

void Oauth2ClientMock::logout(nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler)
{
    Oauth2ClientMockManager::RequestPath path = {
        api::kOauthLogoutPath, nx::network::http::Method::delete_};
    processRequest<void>(path, std::move(completionHandler), Oauth2MockResult{});
}

void Oauth2ClientMock::getJwtPublicKeys(
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
        completionHandler)
{
    Oauth2ClientMockManager::RequestPath path = {
        api::kOauthJwksPath, nx::network::http::Method::get};
    processRequest<std::vector<nx::network::jwk::Key>>(path, std::move(completionHandler), Oauth2MockResult{});
}

void Oauth2ClientMock::getJwtPublicKeyByKid(
    const std::string& kid,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)> completionHandler)
{
    Oauth2ClientMockManager::RequestPath path = {
        nx::network::http::rest::substituteParameters(api::kOauthJwkByIdPath, {kid}),
        nx::network::http::Method::get};
    processRequest<std::string, nx::network::jwk::Key>(path, kid, std::move(completionHandler));
}

void Oauth2ClientMock::introspectSession(
    const std::string& session,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)> completionHandler)
{
    Oauth2ClientMockManager::RequestPath path = {
        nx::network::http::rest::substituteParameters(api::kOauthSessionPath, {session}),
        nx::network::http::Method::get};
    processRequest<std::string, api::GetSessionResponse>(
        path, session, std::move(completionHandler));
}

void Oauth2ClientMock::markSessionMfaVerified(
    const std::string& sessionId, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler)
{
    Oauth2ClientMockManager::RequestPath path = {
        nx::network::http::rest::substituteParameters(api::kOauthSessionPath, {sessionId}),
        nx::network::http::Method::post};
    processRequest<std::string, void>(
        path, sessionId, std::move(handler));
}

void Oauth2ClientMock::notifyAccountUpdated(
    const db::api::AccountChangedEvent& /*event*/,
    nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler)
{
    // We don't want to check this handler in most cdb tests
    return completionHandler(db::api::ResultCode::ok);
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

void Oauth2ClientMockManager::setResponse(const RequestPath& requestPath, const Response& response)
{
    m_responses[requestPath] = response;
}

void Oauth2ClientMockManager::setRequest(const RequestPath& requestPath, const std::string& request)
{
    m_requests[requestPath] = request;
}

void Oauth2ClientMockManager::setRequestPattern(
    const RequestPathRegex& requestPath,
    const Response& response)
{
    m_requestPatterns.emplace_back(std::make_pair(requestPath, response));
}

int Oauth2ClientMockManager::getNumCalls(const RequestPath& requestPath)
{
    return m_requestsCounter[requestPath];
}

} // namespace nx::cloud::oauth2::client::test
