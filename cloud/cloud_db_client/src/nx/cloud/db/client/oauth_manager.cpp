// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_manager.h"

#include "data/oauth_data.h"
#include "cdb_request_path.h"

#include <nx/network/http/rest/http_rest_client.h>

namespace nx::cloud::db::client {

OauthManager::OauthManager(network::cloud::CloudModuleUrlFetcher* cloudModuleUrlFetcher):
    AsyncRequestsExecutor(cloudModuleUrlFetcher)
{
}

void OauthManager::issueToken(
    const api::IssueTokenRequest& request,
    nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueTokenResponse)> completionHandler)
{
    executeRequest<api::IssueTokenResponse>(
        nx::network::http::Method::post,
        kOauthTokenPath,
        request,
        std::move(completionHandler));
}

void OauthManager::issueAuthorizationCode(
    const api::IssueTokenRequest& request,
    nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueCodeResponse)> completionHandler)
{
    executeRequest<api::IssueCodeResponse>(
        nx::network::http::Method::post,
        kOauthTokenPath,
        request,
        std::move(completionHandler));
}

void OauthManager::legacyValidateToken(
    const std::string& token,
    nx::utils::MoveOnlyFunc<void(api::ResultCode, api::ValidateTokenResponse)> completionHandler)
{
    auto requestPath =
        nx::network::http::rest::substituteParameters(kOauthTokenValidatePath, {token});

    executeRequest<api::ValidateTokenResponse>(
        nx::network::http::Method::get,
        requestPath,
        std::move(completionHandler));
}

void OauthManager::introspectToken(
    const api::TokenIntrospectionRequest request,
    nx::utils::MoveOnlyFunc<void(api::ResultCode, api::TokenIntrospectionResponse)> handler)
{
    executeRequest<api::TokenIntrospectionResponse>(
        nx::network::http::Method::post,
        kOauthIntrospectPath,
        request,
        std::move(handler));
}

void OauthManager::deleteToken(
    const std::string& token, nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    auto requestPath =
        nx::network::http::rest::substituteParameters(kOauthTokenValidatePath, {token});

    executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_,
        requestPath,
        std::move(completionHandler));
}

void OauthManager::deleteTokens(
    const std::string& clientId, nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    auto requestPath =
        nx::network::http::rest::substituteParameters(kOauthTokensDeletePath, {clientId});

    executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_,
        requestPath,
        std::move(completionHandler));
}

void OauthManager::logout(nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_,
        kOauthLogoutPath,
        std::move(completionHandler));
}

void OauthManager::issueStunToken(const api::IssueStunTokenRequest& request,
    nx::utils::MoveOnlyFunc<void(api::ResultCode, api::IssueStunTokenResponse)> completionHandler)
{
    executeRequest<api::IssueStunTokenResponse>(
        nx::network::http::Method::post,
        kOauthStunTokenPath,
        request,
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
