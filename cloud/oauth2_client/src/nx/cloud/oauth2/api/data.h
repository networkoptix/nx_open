// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/reflect/instrument.h>

namespace nx::cloud::oauth2::api {

struct GetSessionResponse
{
    /* True if the session exists and valid */
    bool active = false;

    /* True if the session was verified with a second factor*/
    bool mfaVerified = false;
};

NX_REFLECTION_INSTRUMENT(GetSessionResponse, (active)(mfaVerified))

struct IssueServiceTokenResponse
{
    /**%apidoc The access token. This is an opaque string.*/
    std::string access_token;

    /**%apidoc Number of seconds the token will expire in.*/
    std::chrono::seconds expires_in;

    /**%apidoc Token expiration time. Milliseconds since the epoch, UTC.*/
    std::chrono::milliseconds expires_at;

    /**%apidoc Must be bearer.*/
    db::api::TokenType token_type;

    /**%apidoc Token access scope.*/
    std::string scope;

    /**%apidoc Error code.*/
    std::optional<std::string> error;
};

NX_REFLECTION_INSTRUMENT(IssueServiceTokenResponse, (access_token)(expires_in)(expires_at) \
    (token_type)(scope)(error))

class NX_OAUTH2_CLIENT_API ServiceTokenClaimSet: public nx::network::jwt::ClaimSet
{
public:
    // Oauth subject type. MUST be InternalService
    std::optional<std::string> subjectTyp() const;
    void setSubjectTyp(const std::string& val);
};

struct IssueServiceTokenRequest
{
    /**%apidoc The desired scope for a token. Only system scope is allowed */
    std::string scope;
};

NX_REFLECTION_INSTRUMENT(IssueServiceTokenRequest, (scope))

} // namespace nx::cloud::oauth2::api
