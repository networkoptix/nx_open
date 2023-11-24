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
    bool needValidateToken = false;

    bool empty() const
    {
        return credentials.authToken.empty()
            && refreshToken.empty()
            && authorizationCode.empty();
    }
};

struct CloudBindData
{
    /**%apidoc Globally unique id of System assigned by Cloud. */
    QString systemId;

    /**%apidoc Key, System uses to authenticate requests to any Cloud module. */
    QString authKey;

    /**%apidoc[opt] Cloud System owner email. Either `owner` or `organizationId` Must be filled. */
    QString owner;

    /**%apidoc[opt] Either `owner` or `organizationId` Must be filled. */
    QString organizationId;
};

} // namespace nx::vms::client::core
