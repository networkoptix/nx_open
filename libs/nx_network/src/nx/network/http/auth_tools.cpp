#include "auth_tools.h"

#include <cinttypes>

#include <openssl/md5.h>

#include <QtCore/QCryptographicHash>

namespace nx {
namespace network {
namespace http {

void AuthToken::setPassword(const nx::String& password)
{
    type = AuthTokenType::password;
    value = password;
}

void AuthToken::setHa1(const nx::String& ha1)
{
    type = AuthTokenType::ha1;
    value = ha1;
}

bool AuthToken::empty() const
{
    return type == AuthTokenType::none || value.isEmpty();
}

PasswordAuthToken::PasswordAuthToken(const nx::String& password)
{
    setPassword(password);
}

Ha1AuthToken::Ha1AuthToken(const nx::String& ha1)
{
    setHa1(ha1);
}

Credentials::Credentials(
    const QString& username,
    const AuthToken& authToken)
    :
    username(username),
    authToken(authToken)
{
}

//-------------------------------------------------------------------------------------------------

namespace {

static void extractValues(
    const AuthToken& authToken,
    boost::optional<StringType>* userPassword,
    boost::optional<BufferType>* predefinedHa1)
{
    if (authToken.type == AuthTokenType::password)
        *userPassword = authToken.value;
    else if (authToken.type == AuthTokenType::ha1)
        *predefinedHa1 = authToken.value;
}

} // namespace

std::optional<header::Authorization> generateAuthorization(
    const Request& request,
    const Credentials& credentials,
    const header::WWWAuthenticate& wwwAuthenticateHeader)
{
    if (wwwAuthenticateHeader.authScheme == header::AuthScheme::basic &&
        (credentials.authToken.type == AuthTokenType::password || credentials.authToken.empty()))
    {
        header::BasicAuthorization basicAuthorization(
            credentials.username.toUtf8(),
            credentials.authToken.value);
        return basicAuthorization;
    }
    else if (wwwAuthenticateHeader.authScheme == header::AuthScheme::digest)
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
	// TODO: #ak This is incorrect value for "uri" actually. See [rfc7616#3.4] and [rfc7230#5.5].
    // The proper fix may be incompatible with some buggy cameras.
    // But, without a proper fix we can run into incompatibility with some HTTP servers.
    const auto effectiveUri = request.requestLine.url.toString(
        QUrl::RemoveScheme | QUrl::RemoveAuthority | QUrl::FullyEncoded).toUtf8();

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

boost::optional<QCryptographicHash::Algorithm> parseAlgorithm(
    const StringType& algorithm)
{
    // TODO: #mux Perhaps we should support some other algorithms?
    if (algorithm.toUpper() == "MD5") return QCryptographicHash::Md5;

    if (algorithm.toUpper() == "SHA-256") return QCryptographicHash::Sha256;

    if (algorithm.isEmpty()) return QCryptographicHash::Md5; //< default
    return boost::none;
}

BufferType calcHa1(
    const StringType& userName,
    const StringType& realm,
    const StringType& userPassword,
    const StringType& algorithm)
{
    QCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
    md5HashCalc.addData(userName);
    md5HashCalc.addData(":");
    md5HashCalc.addData(realm);
    md5HashCalc.addData(":");
    md5HashCalc.addData(userPassword);
    return md5HashCalc.result().toHex();
}

BufferType calcHa1(
    const QString& userName,
    const QString& realm,
    const QString& userPassword,
    const QString& algorithm)
{
    return calcHa1(
        userName.toUtf8(), realm.toUtf8(),
        userPassword.toUtf8(), algorithm.toUtf8());
}

BufferType calcHa1(
    const char* userName,
    const char* realm,
    const char* userPassword,
    const char* algorithm)
{
    return calcHa1(
        StringType(userName), StringType(realm),
        StringType(userPassword), StringType(algorithm));
}

BufferType calcHa2(
    const StringType& method,
    const StringType& uri,
    const StringType& algorithm)
{
    QCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
    md5HashCalc.addData(method);
    md5HashCalc.addData(":");
    md5HashCalc.addData(uri);
    return md5HashCalc.result().toHex();
}

BufferType calcResponse(
    const BufferType& ha1,
    const BufferType& nonce,
    const BufferType& ha2,
    const StringType& algorithm)
{
    QCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
    md5HashCalc.addData(ha1);
    md5HashCalc.addData(":");
    md5HashCalc.addData(nonce);
    md5HashCalc.addData(":");
    md5HashCalc.addData(ha2);
    return md5HashCalc.result().toHex();
}

BufferType calcResponseAuthInt(
    const BufferType& ha1,
    const BufferType& nonce,
    const StringType& nonceCount,
    const BufferType& clientNonce,
    const StringType& qop,
    const BufferType& ha2,
    const StringType& algorithm)
{
    QCryptographicHash md5HashCalc(*parseAlgorithm(algorithm));
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
    return md5HashCalc.result().toHex();
}


static BufferType fieldOrDefault(
    const std::map<BufferType, BufferType>& inputParams,
    const BufferType& name,
    const BufferType& defaultValue = BufferType())
{
    const auto iter = inputParams.find(name);
    return iter != inputParams.end() ? iter->second : defaultValue;
}

static bool isQopTypeSupported(const BufferType& qopAttributeValue)
{
    if (qopAttributeValue.isEmpty())
        return true;

    for (const auto& qopOption: qopAttributeValue.split(','))
    {
        if (qopOption.trimmed().toLower() == "auth")
            return true;
    }

    return false;
}

static bool calcDigestResponse(
    const QByteArray& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHa1,
    const StringType& uri,
    const std::map<BufferType, BufferType>& inputParams,
    std::map<BufferType, BufferType>* const outputParams)
{
    const auto algorithm = fieldOrDefault(inputParams, "algorithm");
    if (!parseAlgorithm(algorithm))
        return false; //< such algorithm is not supported

    const auto nonce = fieldOrDefault(inputParams, "nonce");
    const auto realm = fieldOrDefault(inputParams, "realm");
    const auto qop = fieldOrDefault(inputParams, "qop");

    if (!isQopTypeSupported(qop))
        return false;

    const BufferType& ha1 = predefinedHa1
        ? predefinedHa1.get()
        : calcHa1(
            userName,
            realm,
            userPassword ? userPassword.get() : QByteArray(),
            algorithm);

    //HA2, qop=auth-int is not supported
    const BufferType& ha2 = calcHa2(method, uri, algorithm);

    //response
    if (!algorithm.isEmpty())
        outputParams->emplace("algorithm", algorithm);

    outputParams->emplace("username", userName);
    outputParams->emplace("realm", realm);
    outputParams->emplace("nonce", nonce);
    outputParams->emplace("uri", uri);

    QByteArray digestResponse;
    if (qop.isEmpty())
    {
        digestResponse = calcResponse(ha1, nonce, ha2, algorithm);
    }
    else
    {
        const BufferType nonceCount = fieldOrDefault(inputParams, "nc", "00000001");
        const BufferType clientNonce = "0a4f113b";    //TODO #ak generate it

        digestResponse = calcResponseAuthInt(
            ha1, nonce, nonceCount, clientNonce, qop, ha2, algorithm);

        outputParams->emplace("qop", qop);
        outputParams->emplace("nc", nonceCount);
        outputParams->emplace("cnonce", clientNonce);
    }
    outputParams->emplace("response", digestResponse);
    return true;
}

bool calcDigestResponse(
    const QByteArray& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHa1,
    const StringType& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader,
    int nonceCount)
{
    if (wwwAuthenticateHeader.authScheme != header::AuthScheme::digest)
        return false;

    // TODO #ak have to set digestAuthorizationHeader->digest->userid somewhere

    auto params = wwwAuthenticateHeader.params;
    StringType nc;
    nc.resize(9);
    nc.resize(std::snprintf(nc.data(), nc.size(), "%08" PRIx32, nonceCount));
    params.emplace("nc", nc);

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
    const StringType& method,
    const Credentials& credentials,
    const StringType& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader,
    int nonceCount)
{
    boost::optional<StringType> userPassword;
    boost::optional<BufferType> predefinedHa1;
    extractValues(credentials.authToken, &userPassword, &predefinedHa1);

    return calcDigestResponse(
        method,
        credentials.username.toUtf8(),
        userPassword,
        predefinedHa1,
        uri,
        wwwAuthenticateHeader,
        digestAuthorizationHeader,
        nonceCount);
}

bool validateAuthorization(
    const StringType& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHa1,
    const header::DigestAuthorization& digestAuthorizationHeader)
{
    const auto& digestParams = digestAuthorizationHeader.digest->params;

    const auto uri = fieldOrDefault(digestParams, "uri");
    if (uri.isEmpty())
        return false;

    const auto algorithm = fieldOrDefault(digestParams, "algorithm");
    if (!parseAlgorithm(algorithm))
        return false; //< Such algorithm is not supported.

    std::map<BufferType, BufferType> outputParams;
    const auto result = calcDigestResponse(
        method, userName, userPassword, predefinedHa1, uri,
        digestParams, &outputParams);

    if (!result)
        return false;

    const auto calculatedResponse = fieldOrDefault(outputParams, "response");
    const auto response = fieldOrDefault(digestParams, "response");
    return !response.isEmpty() && response == calculatedResponse;
}

bool validateAuthorization(
    const StringType& method,
    const Credentials& credentials,
    const header::DigestAuthorization& digestAuthorizationHeader)
{
    return validateAuthorization(
        method,
        credentials.username.toUtf8(),
        credentials.authToken.type == AuthTokenType::password
            ? boost::make_optional(credentials.authToken.value)
            : boost::none,
        credentials.authToken.type == AuthTokenType::ha1
            ? boost::make_optional(credentials.authToken.value)
            : boost::none,
        digestAuthorizationHeader);
}

static const size_t MD5_CHUNK_LEN = 64;

BufferType calcIntermediateResponse(
    const BufferType& ha1,
    const BufferType& nonce)
{
    NX_ASSERT((ha1.size() + 1 + nonce.size()) % MD5_CHUNK_LEN == 0);

    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);
    MD5_Update(&md5Ctx, ha1.constData(), ha1.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, nonce.constData(), nonce.size());
    BufferType intermediateResponse;
    intermediateResponse.resize(MD5_DIGEST_LENGTH);
    memcpy(intermediateResponse.data(), &md5Ctx, MD5_DIGEST_LENGTH);
    return intermediateResponse.toHex();
}

BufferType calcResponseFromIntermediate(
    const BufferType& intermediateResponse,
    size_t intermediateResponseNonceLen,
    const BufferType& nonceTrailer,
    const BufferType& ha2)
{
    NX_ASSERT((MD5_DIGEST_LENGTH * 2 + 1 + intermediateResponseNonceLen) % MD5_CHUNK_LEN == 0);

    const auto intermediateResponseBin = QByteArray::fromHex(intermediateResponse);

    MD5_CTX md5Ctx;
    memset(&md5Ctx, 0, sizeof(md5Ctx));
    memcpy(&md5Ctx, intermediateResponseBin.constData(), MD5_DIGEST_LENGTH);
    md5Ctx.Nl = (MD5_DIGEST_LENGTH * 2 + 1 + intermediateResponseNonceLen) << 3;
    // As if we have just passed ha1:nonceBase to MD5_Update.
    MD5_Update(&md5Ctx, nonceTrailer.constData(), nonceTrailer.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, ha2.constData(), ha2.size());
    BufferType response;
    response.resize(MD5_DIGEST_LENGTH);
    MD5_Final(reinterpret_cast<unsigned char*>(response.data()), &md5Ctx);
    return response.toHex();
}

} // namespace nx
} // namespace network
} // namespace http
