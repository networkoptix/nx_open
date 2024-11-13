// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>

#include <nx/network/jose/jwt.h>
#include <nx/reflect/instrument.h>

#include "two_factor_auth_data.h"

namespace nx::cloud::db::api {

// Code style violated in fields names to match RFC
// and support correct API documentation generation
NX_REFLECTION_ENUM_CLASS(GrantType,
    password,
    refresh_token,
    authorization_code,
    sso_tokens
);

NX_REFLECTION_ENUM_CLASS(ResponseType,
    token,
    code
);

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

    /**%apidoc Valid refresh token. Valid with <pre>grant_type=refresh_token</pre> or
     * <pre>grant_type=sso_tokens</pre>
     */
    std::optional<std::string> refresh_token;

    /**%apidoc Valid authorization code. Valid with <pre>grant_type=code</pre> */
    std::optional<std::string> code;

    /**%apidoc Non-standard extensions. Valid access token. Valid with
     * <pre>grant_type=sso_tokens</pre>
     */
    std::optional<std::string> access_token;
};

NX_REFLECTION_INSTRUMENT(IssueTokenRequest, (grant_type)(response_type)(client_id)(scope) \
    (password)(username)(refresh_token)(code)(refresh_token_lifetime)(access_token))

NX_REFLECTION_ENUM_CLASS(TokenType,
    bearer
);

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

    /**%apidoc A session is identified by a refresh token. It is used to track 2fa state.*/
    std::string session;
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

NX_REFLECTION_INSTRUMENT(
    IssueTokenResponse,
    (access_token)(refresh_token)(expires_in)(expires_at)(token_type)(scope)(error));

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

NX_REFLECTION_INSTRUMENT(IssueCodeResponse, (access_code)(code)(error))

struct ValidateTokenResponse: public TokenInfo
{
    /**%apidoc The username the token belongs to.*/
    std::string username;

    /**%apidoc Seconds. Time that passed since this token was confirmed with a password entry
     * (explicitly or implicitly).<br/>
     * E.g., if an access token was issued with <pre>grant_type=refresh_token</pre>, then
     * its time_since_password will be the time passed since corresponding refresh token was issued
     * with `grant_type=password`. If, in turn the refresh token was issued with `grant_type=code`,
     * then the time passed since issuing the authorization code with `grant_type=password` will be here.
     * And so on.
     */
    std::chrono::seconds time_since_password;
};

NX_REFLECTION_INSTRUMENT(ValidateTokenResponse, (access_token)(expires_in)(expires_at)(token_type) \
    (scope)(username)(time_since_password))

struct TokenIntrospectionRequest
{
    /**%apidoc The token to introspect.*/
    std::string token;

    /**%apidoc Ids of systems to fetch user roles from. */
    std::optional<std::vector<std::string>> system_ids;

    /**%apidoc Skip non-shared systems from system_ids in response.*/
    bool skip_non_shared = false;
};

NX_REFLECTION_INSTRUMENT(TokenIntrospectionRequest, (token)(system_ids)(skip_non_shared))

struct TokenIntrospectionResponse
{
    /**%apidoc true if token is valid. false if token is unknown, expired or revoked. */
    bool active = false;

    /**%apidoc The id of the client which requested the token. */
    std::string client_id;

    /**%apidoc The username associated with the token. */
    std::string username;

    /**%apidoc The token's scope. */
    std::string scope;

    /**%apidoc Seconds since epoch (1970-01-01 UTC) to token expiration time.*/
    std::chrono::seconds exp;

    /**%apidoc Seconds to token expiration.*/
    std::chrono::seconds expires_in;

    /**%apidoc Seconds. Time that passed since this token was confirmed with a password entry
    * (explicitly or implicitly).<br/>
    * E.g., if an access token was issued with <pre>grant_type=refresh_token</pre>, then
    * its time_since_password will be the time passed since corresponding refresh token was issued
    * with `grant_type=password`. If, in turn the refresh token was issued with `grant_type=code`,
    * then the time passed since issuing the authorization code with `grant_type=password` will be here.
    * And so on.
    */
    std::chrono::seconds time_since_password;

    TokenType token_type = TokenType::bearer;

    /**%apidoc Specifies the list of role ids assigned to user for each system requested.
     * Present only if the request contained system_ids attribute.
     */
    std::optional<std::map<std::string /*systemId*/, std::vector<std::string> /*roleIds*/>> system_role_ids;
};

NX_REFLECTION_INSTRUMENT(TokenIntrospectionResponse, (active)(client_id)(username)(scope)(exp) \
    (expires_in)(time_since_password)(token_type)(system_role_ids))

struct TokenIntrospectionResponseShort
{
    /**%apidoc true if token is valid. false if token is unknown, expired or revoked. */
    bool active = false;
};

NX_REFLECTION_INSTRUMENT(TokenIntrospectionResponseShort, (active))

struct IssueStunTokenRequest
{
    /**%apidoc Stun server name */
    std::string server_name;
};

NX_REFLECTION_INSTRUMENT(IssueStunTokenRequest, (server_name))

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

NX_REFLECTION_INSTRUMENT(IssueStunTokenResponse, (token)(mac_code)(error)(kid)(expires_at)(expires_in))

/**
 * JWT claims set. Field names extended with field from RFC 9068
 */
class ClaimSet: public nx::network::jwt::ClaimSet
{
public:
    std::optional<std::string> clientId() const;
    void setClientId(const std::string& val);

    // The session Id is defined by refresh token
    std::optional<std::string> sid() const;
    void setSid(const std::string& val);

    // Seconds. Time that passed since this token was confirmed with a password entry
    // either explicitly or implicitly
    std::optional<std::chrono::seconds> passwordTime() const;
    void setPasswordTime(const std::chrono::seconds& val);

    // Oauth token type. MUST be one of accessToken, refreshToken, authCode, pwdReset
    std::optional<std::string> typ() const;
    void setTyp(const std::string& val);

    std::optional<int> securitySequence() const;
    void setSecuritySequence(int val);


    // For internal purpose only. The AWS region the user account belongs to.
    std::optional<std::string> region() const;
    void setRegion(const std::string& val);
};

using Token = nx::network::jws::Token<ClaimSet>;

struct GetAccSecuritySettingsChangedEventsRequest
{
    /**%apidoc The timestamp since which all password reset events should be returned.
    * Seconds since the epoch, UTC.
    */
    std::chrono::seconds timestamp;
};

NX_REFLECTION_INSTRUMENT(GetAccSecuritySettingsChangedEventsRequest, (timestamp))

NX_REFLECTION_ENUM_CLASS(AccSettingChanged,
    enabled2Fa,
    passwordChanged,
    other);

struct AccSecuritySettingsChangedEvent
{
    // Event timestamp, seconds since UTC
    std::chrono::seconds ts;

    // Account email
    std::string email;

    // Account region
    std::string accRegion;

    // Event type
    AccSettingChanged eventType;

    // If eventType == passwrodChanged contains the session which should remain active
    // If eventType == enabled2Fa contains session which should be marked as 2fa verified
    std::optional<std::string> validSession;
};

NX_REFLECTION_INSTRUMENT(AccSecuritySettingsChangedEvent, (ts)(email)(accRegion)(eventType)(validSession))

using GetAccSecuritySettingsChangedEventsResponse = std::vector<AccSecuritySettingsChangedEvent>;

struct AuthSession
{
    /**%apidoc Session id.*/
    std::string id;

    /**%apidoc The username associated with the session. */
    std::string username;

    /**%apidoc Client id. */
    std::string clientId;

    /**%apidoc The session creation time in seconds since UTC. */
    std::chrono::system_clock::time_point startTime;

    /**%apidoc The session expiration time in seconds since UTC. */
    std::chrono::system_clock::time_point expirationTime;

    /**%apidoc True if mfa was passed for the session, false otherwise */
    SecondFactorState mfaStatus = SecondFactorState::notEntered;
};

NX_REFLECTION_INSTRUMENT(AuthSession, (id)(username)(clientId)(startTime)(expirationTime) \
    (mfaStatus))

} // namespace nx::cloud::db::api
