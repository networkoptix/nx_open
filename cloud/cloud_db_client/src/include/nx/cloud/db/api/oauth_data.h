// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace nx::cloud::db::api {

enum class GrantType
{
    password,
    refreshToken,
    authorizationCode,
};

enum class ResponseType
{
    token,
    code,
};

struct IssueTokenRequest
{
    /**%apidoc Depending on the grant type, a corresponding attribute (password, refresh_token, code)
     * is expected to be specified.
     */
    GrantType grant_type = GrantType::password;

    ResponseType response_type = ResponseType::token;
    std::string client_id;

    /**%apidoc The desired scope for new token. If grant_type is <i>refresh_token</i> or <i>code</i> */
    std::optional<std::string> scope;

    /**%apidoc Refresh token life time in seconds. It may be used to decrease the life time only.
     * So, the value which is greater than the corresponding config value is ignored.
     */
    std::optional<std::chrono::seconds> refresh_token_lifetime;

    /**%apidoc User password. Valid with <pre>grant_type=password</pre> */
    std::optional<std::string> password;

    /**%apidoc Username. Valid with <pre>grant_type=password</pre> */
    std::optional<std::string> username;

    /**%apidoc Valid refresh token. Valid with <pre>grant_type=refresh_token</pre> */
    std::optional<std::string> refresh_token;

    /**%apidoc Valid authorization code. Valid with <pre>grant_type=code</pre> */
    std::optional<std::string> code;
};

enum class TokenType
{
    bearer,
};

struct TokenInfo
{
    /**%apidoc The access token. This is an opaque string.*/
    std::string access_token;

    /**%apidoc Number of seconds the token will expire in.*/
    std::chrono::seconds expires_in;

    /**%apidoc Token expiration time. Milliseconds since the epoch, UTC.*/
    std::chrono::milliseconds expires_at;

    TokenType token_type;

    /**%apidoc Token access scope. It is applied to any token (access token, refresh token,
     * authorization code).
     */
    std::string scope;
};

struct IssueTokenResponse: public TokenInfo
{
    /**%apidoc Refresh token that may be used to request access tokens with the same or a limited
     * scope.
     */
    std::string refresh_token;

    /**%apidoc Error code.*/
    std::optional<std::string> error;
};

struct IssueCodeResponse
{
    /**%apidoc
     * %deprecated Will be removed soon.
     */
    std::string access_code;

    /**%apidoc The OAUTH2 authorization code.*/
    std::string code;

    /**%apidoc Error code.*/
    std::optional<std::string> error;
};

struct ValidateTokenResponse : public TokenInfo
{
    /**%apidoc The username the token belongs to.*/
    std::string username;

    /**%apidoc The VMS user id. Present only if defined for specific account and specific system.*/
    std::optional<std::string> vms_user_id;

    /**%apidoc Seconds. Time that passed since this token was confirmed with a password entry
     * (explicitely or implicitely).<br/>
     * E.g., if an access token was issued with <pre>grant_type=refresh_token</pre>, then
     * its time_since_password will be the time passed since corresponding refresh token was issued
     * with `grant_type=password`. If, in turn the refresh token was issued with `grant_type=code`,
     * then the time passed since issuing the authorization code with `grant_type=password` will be here.
     * And so on.
     */
    std::chrono::seconds time_since_password;
};

struct IssueStunTokenRequest
{
    /**%apidoc Stun server name */
    std::string server_name;
};

struct IssueStunTokenResponse
{
    /**%apidoc The token itself.*/
    std::string token;

    /**%apidoc Mac code used by a VMS server.*/
    std::string mac_code;

    /**%apidoc An ephemeral and unique key identifier.*/
    std::string kid;

    /**%apidoc Token expiration time. Milliseconds since the epoch, UTC.*/
    std::chrono::milliseconds expires_at;

    /**%apidoc Number of seconds the token will expire in.*/
    std::chrono::seconds expires_in;

    /**%apidoc Error code.*/
    std::optional<std::string> error;
};

} // namespace nx::cloud::db::api
