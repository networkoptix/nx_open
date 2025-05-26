// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../oauth2_client.h"

namespace nx::cloud::oauth2::client::test {

// The class added to provide convenient access to legacy cdb handlers moved to Oauth2 without
// adding them to public client
class NX_OAUTH2_CLIENT_API Oauth2ClientWithLegacyHandlers: public Oauth2Client
{
    using base_type = nx::network::http::GenericApiClient<db::client::ResultCodeDescriptor>;

public:
    Oauth2ClientWithLegacyHandlers(
        const nx::Url& url,
        const std::optional<nx::network::http::Credentials>& credentials);

    void legacyValidateToken(
        const std::string& token,
        std::function<void(db::api::ResultCode, db::api::ValidateTokenResponse)> completionHandler);

    void deleteToken(
        const std::string& token, std::function<void(db::api::ResultCode)> completionHandler);
};

} // namespace nx::cloud::oauth2::client::test
