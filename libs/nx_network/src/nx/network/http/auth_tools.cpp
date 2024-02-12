// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_tools.h"

#include <cinttypes>

#include <QtNetwork/QAuthenticator>

#include <openssl/md5.h>

#include <nx/reflect/json.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/random.h>
#include <nx/utils/random_cryptographic_device.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>

namespace nx::network::http {

void AuthToken::setPassword(const std::string_view& password)
{
    type = AuthTokenType::password;
    value = password;
}

bool AuthToken::isPassword() const
{
    return type == AuthTokenType::password;
}

void AuthToken::setHa1(const std::string_view& ha1)
{
    type = AuthTokenType::ha1;
    value = ha1;
}

bool AuthToken::isHa1() const
{
    return type == AuthTokenType::ha1;
}

void AuthToken::setBearerToken(const std::string_view& token)
{
    type = AuthTokenType::bearer;
    value = token;
}

bool AuthToken::isBearerToken() const
{
    return type == AuthTokenType::bearer;
}

bool AuthToken::empty() const
{
    return type == AuthTokenType::none || value.empty();
}

PasswordAuthToken::PasswordAuthToken(const std::string_view& password)
{
    setPassword(password);
}

Ha1AuthToken::Ha1AuthToken(const std::string_view& ha1)
{
    setHa1(ha1);
}

BearerAuthToken::BearerAuthToken(const std::string_view& token)
{
    setBearerToken(token);
}

VideoWallAuthToken::VideoWallAuthToken(const nx::Uuid& videoWallId)
{
    setBearerToken(prefix + videoWallId.toStdString());
}

Credentials::Credentials(
    const std::string_view& username,
    const AuthToken& authToken)
    :
    username(username),
    authToken(authToken)
{
}

Credentials::Credentials(const BearerAuthToken& authToken):
    authToken(authToken)
{
}

Credentials::Credentials(const QAuthenticator& credentials):
    username(credentials.user().toStdString()),
    authToken(PasswordAuthToken(credentials.password().toStdString()))
{
}

void PrintTo(const Credentials& val, ::std::ostream* os)
{
    *os << nx::reflect::json::serialize(SerializableCredentials(val));
}

//-------------------------------------------------------------------------------------------------

SerializableCredentials::SerializableCredentials(std::string user):
    user(std::move(user))
{
}

SerializableCredentials::SerializableCredentials(const Credentials& credentials):
    user(credentials.username)
{
    switch (credentials.authToken.type)
    {
        case AuthTokenType::password:
            password = credentials.authToken.value;
            break;
        case AuthTokenType::ha1:
            ha1 = credentials.authToken.value;
            break;
        case AuthTokenType::bearer:
            token = credentials.authToken.value;
            break;
        default:
            break;
    }
}

SerializableCredentials::operator Credentials() const
{
    if (token)
        return Credentials(user, BearerAuthToken(*token));

    if (!user.empty() && ha1)
        return Credentials(user, Ha1AuthToken(*ha1));

    if (!user.empty() && password)
        return PasswordCredentials(user, *password);

    return Credentials(user, {});
}

//-------------------------------------------------------------------------------------------------

namespace {

static void extractValues(
    const AuthToken& authToken,
    std::optional<std::string>* userPassword,
    std::optional<std::string>* predefinedHa1)
{
    if (authToken.isPassword())
        *userPassword = authToken.value;
    else if (authToken.isHa1())
        *predefinedHa1 = authToken.value;
}

} // namespace

std::optional<header::Authorization> generateAuthorization(
    const Request& request,
    const Credentials& credentials,
    const header::WWWAuthenticate& wwwAuthenticateHeader)
{
    if (credentials.authToken.isBearerToken())
    {
        header::BearerAuthorization bearerAuthorization(credentials.authToken.value);
        return bearerAuthorization;
    }

    if (wwwAuthenticateHeader.authScheme == header::AuthScheme::basic &&
        (credentials.authToken.isPassword() || credentials.authToken.empty()))
    {
        header::BasicAuthorization basicAuthorization(
            credentials.username,
            credentials.authToken.value);
        return basicAuthorization;
    }

    if (wwwAuthenticateHeader.authScheme == header::AuthScheme::digest)
    {
        return generateDigestAuthorization(
            request,
            credentials,
            wwwAuthenticateHeader,
            1);
    }

    return std::nullopt;
}

NX_NETWORK_API std::optional<header::Authorization> generateDigestAuthorization(
    const Request& request,
    const Credentials& credentials,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    int nonceCount)
{
    const auto effectiveUri = digestUri(request.requestLine.method, request.requestLine.url);

    header::DigestAuthorization digestAuthorizationHeader;
    if (!calcDigestResponse(
            request.requestLine.method,
            credentials,
            effectiveUri,
            wwwAuthenticateHeader,
            &digestAuthorizationHeader,
            nonceCount))
    {
        return std::nullopt;
    }

    return digestAuthorizationHeader;
}

std::optional<nx::utils::QnCryptographicHash::Algorithm> parseAlgorithm(
    const std::string_view& algorithm)
{
    // TODO: #muskov Perhaps we should support some other algorithms?
    if (nx::utils::stricmp(algorithm, "MD5") == 0)
        return nx::utils::QnCryptographicHash::Md5;

    if (nx::utils::stricmp(algorithm, "SHA-256") == 0)
        return nx::utils::QnCryptographicHash::Sha256;

    if (algorithm.empty())
        return nx::utils::QnCryptographicHash::Md5; //< default

    return std::nullopt;
}

std::string calcHa1(
    const std::string_view& userName,
    const std::string_view& realm,
    const std::string_view& userPassword,
    const std::string_view& algorithm)
{
    nx::utils::QnCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
    md5HashCalc.addData(userName);
    md5HashCalc.addData(":");
    md5HashCalc.addData(realm);
    md5HashCalc.addData(":");
    md5HashCalc.addData(userPassword);
    const auto hash = md5HashCalc.result();
    return nx::utils::toHex(std::string_view(hash.data(), (std::size_t) hash.size()));
}

std::string calcHa2(
    const Method& method,
    const std::string_view& uri,
    const std::string_view& algorithm)
{
    // TODO: #akolesnikov Use openssl directly.

    nx::utils::QnCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
    md5HashCalc.addData(method.toString());
    md5HashCalc.addData(":");
    md5HashCalc.addData(uri.data());
    const auto hash = md5HashCalc.result();
    return nx::utils::toHex(std::string_view(hash.data(), (std::size_t) hash.size()));
}

std::string calcResponse(
    const std::string_view& ha1,
    const std::string_view& nonce,
    const std::string_view& ha2,
    const std::string_view& algorithm)
{
    nx::utils::QnCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
    md5HashCalc.addData(ha1);
    md5HashCalc.addData(":");
    md5HashCalc.addData(nonce);
    md5HashCalc.addData(":");
    md5HashCalc.addData(ha2);
    const auto hash = md5HashCalc.result();
    return nx::utils::toHex(std::string_view(hash.data(), (std::size_t) hash.size()));
}

std::string calcResponseAuthInt(
    const std::string_view& ha1,
    const std::string_view& nonce,
    const std::string_view& nonceCount,
    const std::string_view& clientNonce,
    const std::string_view& qop,
    const std::string_view& ha2,
    const std::string_view& algorithm)
{
    nx::utils::QnCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
    md5HashCalc.addData(ha1);
    md5HashCalc.addData(":");
    md5HashCalc.addData(nonce);
    md5HashCalc.addData(":");
    md5HashCalc.addData(nonceCount);
    md5HashCalc.addData(":");
    md5HashCalc.addData(clientNonce);
    md5HashCalc.addData(":");
    md5HashCalc.addData(qop);
    md5HashCalc.addData(":");
    md5HashCalc.addData(ha2);
    const auto hash = md5HashCalc.result();
    return nx::utils::toHex(std::string_view(hash.data(), (std::size_t) hash.size()));
}

static std::string fieldOrDefault(
    const std::map<std::string, std::string>& inputParams,
    const std::string& name,
    const std::string& defaultValue = std::string())
{
    const auto iter = inputParams.find(name);
    return iter != inputParams.end() ? iter->second : defaultValue;
}

static bool isQopTypeSupported(const std::string_view& qopAttributeValue)
{
    if (qopAttributeValue.empty())
        return true;

    bool found = false;
    nx::utils::split(
        qopAttributeValue, ',',
        [&found](const auto& qopOption)
        {
            found = found || nx::utils::stricmp(nx::utils::trim(qopOption), "auth") == 0;
        });
    return found;
}

bool calcDigestResponse(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const std::string_view& uri,
    const std::map<std::string, std::string>& inputParams,
    std::map<std::string, std::string>* const outputParams)
{
    const auto algorithm = fieldOrDefault(inputParams, "algorithm");
    if (!parseAlgorithm(algorithm))
        return false; //< such algorithm is not supported

    const auto nonce = fieldOrDefault(inputParams, "nonce");
    const auto realm = fieldOrDefault(inputParams, "realm");
    const auto qop = fieldOrDefault(inputParams, "qop");

    if (!isQopTypeSupported(qop))
        return false;

    const std::string ha1 = predefinedHa1
        ? std::string(*predefinedHa1)
        : calcHa1(
            userName,
            realm,
            userPassword ? *userPassword : std::string(),
            algorithm);

    //HA2, qop=auth-int is not supported
    const std::string& ha2 = calcHa2(method, uri, algorithm);

    //response
    if (!algorithm.empty())
        outputParams->emplace("algorithm", algorithm);

    outputParams->emplace("username", userName);
    outputParams->emplace("realm", realm);
    outputParams->emplace("nonce", nonce);
    outputParams->emplace("uri", uri);

    std::string digestResponse;
    if (qop.empty())
    {
        digestResponse = calcResponse(ha1, nonce, ha2, algorithm);
    }
    else
    {
        std::string nonceCount = fieldOrDefault(inputParams, "nc", "00000001");
        std::string clientNonce = fieldOrDefault(
            inputParams, "cnonce", nx::utils::random::generateName(16));

        digestResponse = calcResponseAuthInt(
            ha1, nonce, nonceCount, clientNonce, qop, ha2, algorithm);

        outputParams->emplace("qop", qop);
        outputParams->emplace("nc", std::move(nonceCount));
        outputParams->emplace("cnonce", clientNonce);
    }
    outputParams->emplace("response", digestResponse);
    return true;
}

bool calcDigestResponse(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const std::string_view& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader,
    int nonceCount)
{
    if (wwwAuthenticateHeader.authScheme != header::AuthScheme::digest)
        return false;

    // TODO #akolesnikov have to set digestAuthorizationHeader->digest->userid somewhere

    auto params = wwwAuthenticateHeader.params;
    std::string nc;
    nc.resize(9);
    nc.resize(std::snprintf(nc.data(), nc.size(), "%08" PRIx32, nonceCount));
    params.emplace("nc", std::move(nc));

    return calcDigestResponse(
        method,
        userName,
        userPassword,
        predefinedHa1,
        uri,
        std::move(params),
        &digestAuthorizationHeader->digest->params);
}

bool calcDigestResponse(
    const Method& method,
    const Credentials& credentials,
    const std::string_view& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader,
    int nonceCount)
{
    std::optional<std::string> userPassword;
    std::optional<std::string> predefinedHa1;
    extractValues(credentials.authToken, &userPassword, &predefinedHa1);

    return calcDigestResponse(
        method,
        credentials.username,
        userPassword,
        predefinedHa1,
        uri,
        wwwAuthenticateHeader,
        digestAuthorizationHeader,
        nonceCount);
}

bool validateAuthorization(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const header::DigestAuthorization& digestAuthorizationHeader)
{
    return validateAuthorization(
        method,
        userName,
        userPassword,
        predefinedHa1,
        *digestAuthorizationHeader.digest);
}

bool validateAuthorization(
    const Method& method,
    const std::string_view& userName,
    const std::optional<std::string_view>& userPassword,
    const std::optional<std::string_view>& predefinedHa1,
    const header::DigestCredentials& digestAuthorizationHeader)
{
    const auto& digestParams = digestAuthorizationHeader.params;

    const auto uri = fieldOrDefault(digestParams, "uri");
    if (uri.empty())
        return false;

    const auto algorithm = fieldOrDefault(digestParams, "algorithm");
    if (!parseAlgorithm(algorithm))
        return false; //< Such algorithm is not supported.

    std::map<std::string, std::string> outputParams;
    const auto result = calcDigestResponse(
        method, userName, userPassword, predefinedHa1, uri,
        digestParams, &outputParams);

    if (!result)
        return false;

    const auto calculatedResponse = fieldOrDefault(outputParams, "response");
    const auto response = fieldOrDefault(digestParams, "response");
    return !response.empty() && response == calculatedResponse;
}

bool validateAuthorization(
    const Method& method,
    const Credentials& credentials,
    const header::DigestAuthorization& digestAuthorizationHeader)
{
    return validateAuthorization(
        method,
        credentials,
        *digestAuthorizationHeader.digest);
}

bool validateAuthorization(
    const Method& method,
    const Credentials& credentials,
    const header::DigestCredentials& digestAuthorizationHeader)
{
    return validateAuthorization(
        method,
        credentials.username,
        credentials.authToken.isPassword()
            ? std::make_optional(credentials.authToken.value)
            : std::nullopt,
        credentials.authToken.isHa1()
            ? std::make_optional(credentials.authToken.value)
            : std::nullopt,
        digestAuthorizationHeader);
}

bool validateAuthorizationByIntemerdiateResponse(
    const Method& method,
    const std::string_view& intermediateResponse,
    const std::string_view& baseNonce,
    const header::DigestCredentials& digestAuthorizationHeader)
{
    std::string uri = fieldOrDefault(digestAuthorizationHeader.params, "uri");
    std::string algorithm = fieldOrDefault(digestAuthorizationHeader.params, "algorithm");
    std::string ha2 = calcHa2(method, uri, algorithm);
    std::string nonce = fieldOrDefault(digestAuthorizationHeader.params, "nonce");
    std::string_view nonceTrailer = nonce;
    nonceTrailer.remove_prefix(std::min(nonceTrailer.size(), baseNonce.size()));
    return fieldOrDefault(digestAuthorizationHeader.params, "response") == calcResponseFromIntermediate(
        intermediateResponse, baseNonce.size(), nonceTrailer, ha2);
}

static const size_t MD5_CHUNK_LEN = 64;

std::string calcIntermediateResponse(
    const std::string_view& ha1,
    const std::string_view& nonce)
{
    NX_ASSERT((ha1.size() + 1 + nonce.size()) % MD5_CHUNK_LEN == 0,
        nx::format("ha1.size() = %1, nonce.size() = %2").args(ha1.size(), nonce.size()));

    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);
    MD5_Update(&md5Ctx, ha1.data(), ha1.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, nonce.data(), nonce.size());
    std::string intermediateResponse;
    intermediateResponse.resize(MD5_DIGEST_LENGTH);
    memcpy(intermediateResponse.data(), &md5Ctx, MD5_DIGEST_LENGTH);
    return nx::utils::toHex(intermediateResponse);
}

std::string calcResponseFromIntermediate(
    const std::string_view& intermediateResponse,
    size_t intermediateResponseNonceLen,
    const std::string_view& nonceTrailer,
    const std::string_view& ha2)
{
    NX_ASSERT((MD5_DIGEST_LENGTH * 2 + 1 + intermediateResponseNonceLen) % MD5_CHUNK_LEN == 0);

    const auto intermediateResponseBin = nx::utils::fromHex(intermediateResponse);

    MD5_CTX md5Ctx;
    memset(&md5Ctx, 0, sizeof(md5Ctx));
    memcpy(&md5Ctx, intermediateResponseBin.data(), MD5_DIGEST_LENGTH);
    md5Ctx.Nl = (MD5_DIGEST_LENGTH * 2 + 1 + intermediateResponseNonceLen) << 3;
    // As if we have just passed ha1:nonceBase to MD5_Update.
    MD5_Update(&md5Ctx, nonceTrailer.data(), nonceTrailer.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, ha2.data(), ha2.size());
    std::string response;
    response.resize(MD5_DIGEST_LENGTH);
    MD5_Final(reinterpret_cast<unsigned char*>(response.data()), &md5Ctx);
    return nx::utils::toHex(response);
}

std::string generateNonce(const std::string& eTag)
{
    static constexpr int secretDataLength = 7;

    const auto now = std::chrono::high_resolution_clock::now();
    return nx::utils::buildString(
        now.time_since_epoch().count(), ":",
        eTag.empty() ? nx::Uuid::createUuid().toSimpleStdString() : eTag, ":",
        nx::utils::random::generateName(
            nx::utils::random::CryptographicDevice::instance(),
            secretDataLength));
}

header::WWWAuthenticate generateWwwAuthenticateBasicHeader(
    const std::string& realm)
{
    header::WWWAuthenticate wwwAuthenticate;
    wwwAuthenticate.authScheme = header::AuthScheme::basic;
    wwwAuthenticate.params.emplace("realm", realm);
    return wwwAuthenticate;
}

header::WWWAuthenticate generateWwwAuthenticateDigestHeader(
    const std::string& nonce,
    const std::string& realm)
{
    header::WWWAuthenticate wwwAuthenticate;
    wwwAuthenticate.authScheme = header::AuthScheme::digest;
    wwwAuthenticate.params.emplace("nonce", nonce);
    wwwAuthenticate.params.emplace("realm", realm);
    wwwAuthenticate.params.emplace("algorithm", "MD5");
    return wwwAuthenticate;
}

std::string digestUri(const Method& method, const nx::utils::Url& url)
{
    // TODO: #akolesnikov This is an incorrect value for "uri". See [rfc7616#3.4] and [rfc7230#5.5].
    // The proper fix may be incompatible with some cameras which loosely follow the standards.
    // But without the proper fix we can run into incompatibility with some HTTP servers.
    return (method == Method::connect)
        ? url.authority().toStdString()
        : url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority | QUrl::FullyEncoded)
              .toStdString();
}

} // namespace nx::network::http
