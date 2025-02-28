// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth2_client_with_legacy_handlers.h"

#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/network/http/rest/http_rest_client.h>

namespace nx::cloud::oauth2::client::test {

using namespace db::api;

Oauth2ClientWithLegacyHandlers::Oauth2ClientWithLegacyHandlers(
    const nx::utils::Url& url, const std::optional<nx::network::http::Credentials>& credentials):
    Oauth2Client(url, credentials)
{
}

void Oauth2ClientWithLegacyHandlers::legacyValidateToken(
    const std::string& token,
    std::function<void(db::api::ResultCode, db::api::ValidateTokenResponse)> completionHandler)
{
    base_type::template makeAsyncCall<ValidateTokenResponse>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(db::kOauthTokenValidatePath, {token}),
        {}, // query
        std::move(completionHandler));
}

void Oauth2ClientWithLegacyHandlers::deleteToken(
    const std::string& token, std::function<void(db::api::ResultCode)> completionHandler)
{
    base_type::template makeAsyncCall</*Output*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(db::kOauthTokenValidatePath, {token}),
        {}, // query
        std::move(completionHandler));
}

} // namespace nx::cloud::oauth2::client::test
