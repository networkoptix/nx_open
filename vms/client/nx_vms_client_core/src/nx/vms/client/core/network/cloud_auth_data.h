// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <nx/network/http/auth_tools.h>

namespace nx::vms::client::core {

struct CloudAuthData
{
    // Effective credentials for cloud connection. Username and password / access token.
    nx::network::http::Credentials credentials;
    // Long-term OAuth refresh token.
    std::string refreshToken;
    // Short-term OAuth authorization code.
    std::string authorizationCode;
    // Unix timestamp of access token expiration.
    std::chrono::milliseconds expiresAt = std::chrono::milliseconds::zero();

    bool empty() const
    {
        return credentials.authToken.empty()
            && refreshToken.empty()
            && authorizationCode.empty();
    }
};

} // namespace nx::vms::client::core
