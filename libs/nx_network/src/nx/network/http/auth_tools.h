// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**
 * @file
 * Helper functions for calculating HTTP Digest (rfc2617).
 */

#pragma once

#include <optional>
#include <string>

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "../app_info.h"
#include "http_types.h"

namespace nx::network::http {

NX_REFLECTION_ENUM_CLASS(AuthTokenType,
    none,
    password,
    ha1,
    bearer
)

/**
 * An HTTP auth token. It can be either password, HA1 hash or a bearer token.
 */
class NX_NETWORK_API AuthToken
{
public:
    std::string value;
    AuthTokenType type = AuthTokenType::none;

    void setPassword(const std::string_view& password);
    bool isPassword() const;
    void setHa1(const std::string_view& ha1);
    bool isHa1() const;
    void setBearerToken(const std::string_view& bearer);
    bool isBearerToken() const;
    bool empty() const;

    bool operator==(const AuthToken& other) const = default;
};

class NX_NETWORK_API PasswordAuthToken:
    public AuthToken
{
public:
    PasswordAuthToken(const std::string_view& password);
};

class NX_NETWORK_API Ha1AuthToken:
    public AuthToken
{
public:
    Ha1AuthToken(const std::string_view& ha1);
};

class NX_NETWORK_API BearerAuthToken:
    public AuthToken
{
public:
    BearerAuthToken(const std::string_view& token);
};

class NX_NETWORK_API VideoWallAuthToken:
    public AuthToken
{
public:
    static inline const std::string prefix = "videoWall-";

    VideoWallAuthToken(const QnUuid& videoWallId);
};

class NX_NETWORK_API Credentials
{
public:
    std::string username;
    AuthToken authToken;

    Credentials() = default;
    Credentials(const std::string_view& username, const AuthToken& authToken);
    Credentials(const BearerAuthToken& authToken);

    // TODO: #akolesnikov get rid of this constructor. Credentials can be used instead of QAuthenticator
    // across the code.
    Credentials(const QAuthenticator& credentials);

    bool operator==(const Credentials& other) const = default;
};

/** GTest support. */
NX_NETWORK_API void PrintTo(const Credentials& val, ::std::ostream* os);

class NX_NETWORK_API PasswordCredentials: public Credentials
{
public:
    PasswordCredentials(const std::string_view& username, const std::string_view& password):
        Credentials(username, PasswordAuthToken(password))
    {
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * Data structure to store and load credentials in a way compatible with Qt. This representation
 * is also more user-friendly, so can be used in public api.
 */
struct NX_NETWORK_API SerializableCredentials
{
    std::string user;
    std::optional<std::string> password;
    std::optional<std::string> ha1;
    std::optional<std::string> token;

    SerializableCredentials() = default;
    SerializableCredentials(std::string user);
    SerializableCredentials(const Credentials& credentials);

    operator Credentials() const;
    bool operator==(const SerializableCredentials& other) const = default;
};
#define SerializableCredentials_Fields (user)(password)(ha1)(token)
NX_REFLECTION_INSTRUMENT(SerializableCredentials, SerializableCredentials_Fields);

//-------------------------------------------------------------------------------------------------

struct AuthInfo
{
    Credentials credentials;
    Credentials proxyCredentials;
    // TODO: #akolesnikov Remove proxyEndpoint and isProxySecure from here.
    SocketAddress proxyEndpoint;
    bool isProxySecure = false;
};

//-------------------------------------------------------------------------------------------------

NX_NETWORK_API std::optional<header::Authorization> generateAuthorization(
    const Request& request,
    const Credentials& credentials,
    const header::WWWAuthenticate& wwwAuthenticateHeader);

NX_NETWORK_API std::optional<header::Authorization> generateDigestAuthorization(
    const Request& request,
    const Credentials& credentials,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    int nonceCount);

NX_NETWORK_API std::string calcHa1(
    const std::string_view& userName,
    const std::string_view& realm,
    const std::string_view& userPassword,
    const std::string_view& algorithm = {});

NX_NETWORK_API std::string calcHa2(
    const Method& method,
    const std::string_view& uri,
    const std::string_view& algorithm = {});

NX_NETWORK_API std::string calcResponse(
    const std::string_view& ha1,
    const std::string_view& nonce,
    const std::string_view& ha2,
    const std::string_view& algorithm = {});

NX_NETWORK_API std::string calcResponseAuthInt(
    const std::string_view& ha1,
    const std::string_view& nonce,
    const std::string_view& nonceCount,
    const std::string_view& clientNonce,
    const std::string_view& qop,
    const std::string_view& ha2,
    const std::string_view& algorithm = {});

/**
 * NOTE: If predefinedHa1 is present, then it is used. Otherwise, HA1 is calculated based on userPassword.
 */
NX_NETWORK_API bool calcDigestResponse(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const std::string_view& uri,
    const std::map<std::string, std::string>& inputParams,
    std::map<std::string, std::string>* outputParams);

NX_NETWORK_API bool calcDigestResponse(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const std::string_view& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader,
    int nonceCount = 1);

bool NX_NETWORK_API calcDigestResponse(
    const Method& method,
    const Credentials& credentials,
    const std::string_view& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader,
    int nonceCount = 1);

/**
 * To be used by server to validate received Authorization against known credentials.
 */
NX_NETWORK_API bool validateAuthorization(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const header::DigestAuthorization& digestAuthorizationHeader);

NX_NETWORK_API bool validateAuthorization(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const header::DigestCredentials& digestAuthorizationHeader);

NX_NETWORK_API bool validateAuthorization(
    const Method& method,
    const Credentials& credentials,
    const header::DigestAuthorization& digestAuthorizationHeader);

NX_NETWORK_API bool validateAuthorization(
    const Method& method,
    const Credentials& credentials,
    const header::DigestCredentials& digestAuthorizationHeader);

NX_NETWORK_API bool validateAuthorizationByIntemerdiateResponse(
    const Method& method,
    const std::string_view& intermediateResponse,
    const std::string_view& baseNonce,
    const header::DigestCredentials& digestAuthorizationHeader);

/**
 * @param ha1 That's what calcHa1 has returned.
 * WARNING: ha1.size() + 1 + nonce.size() MUST be divisible by 64!
 *   This is requirement of MD5 algorithm.
 */
NX_NETWORK_API std::string calcIntermediateResponse(
    const std::string_view& ha1,
    const std::string_view& nonce);

/**
 * Calculates MD5(ha1:nonce:ha2).
 * nonce is concatenation of nonceBase and nonceTrailer.
 * intermediateResponse is result of nx::network::http::calcIntermediateResponse.
 * @param intermediateResponse Calculated with calcIntermediateResponse.
 * @param intermediateResponseNonceLen Length of nonce (bytes) used to generate intermediateResponse.
 */
NX_NETWORK_API std::string calcResponseFromIntermediate(
    const std::string_view& intermediateResponse,
    size_t intermediateResponseNonceLen,
    const std::string_view& nonceTrailer,
    const std::string_view& ha2);

/**
 * Generates nonce according to rfc7616.
 */
NX_NETWORK_API std::string generateNonce(
    const std::string& eTag = std::string());

NX_NETWORK_API header::WWWAuthenticate generateWwwAuthenticateBasicHeader(
    const std::string& realm = AppInfo::realm());

NX_NETWORK_API header::WWWAuthenticate generateWwwAuthenticateDigestHeader(
    const std::string& nonce,
    const std::string& realm = AppInfo::realm());

NX_NETWORK_API std::string digestUri(const Method& method, const nx::utils::Url& url);

//-------------------------------------------------------------------------------------------------

/**
 * Interface of a class-source of HTTP credentials.
 */
class NX_NETWORK_API AbstractCredentialsProvider
{
public:
    struct Result
    {
        Credentials credentials;
        std::optional<std::chrono::system_clock::time_point> expirationTime;
    };

    virtual ~AbstractCredentialsProvider() = default;

    /**
     * Returns the credentials or std::nullopt if there are no credentials at this moment.
     * It is recommended that the implementation is thread-safe.
     */
    virtual std::optional<Result> get() const = 0;
};

} // namespace nx::network::http
