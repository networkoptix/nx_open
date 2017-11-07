#include "auth_tools.h"

#include <openssl/md5.h>

#include <QtCore/QCryptographicHash>

namespace nx_http {

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

bool addAuthorization(
    Request* const request,
    const Credentials& credentials,
    const header::WWWAuthenticate& wwwAuthenticateHeader)
{
    if (wwwAuthenticateHeader.authScheme == header::AuthScheme::basic &&
        (credentials.authToken.type == AuthTokenType::password || credentials.authToken.empty()))
    {
        header::BasicAuthorization basicAuthorization(
            credentials.username.toUtf8(),
            credentials.authToken.value);
        nx_http::insertOrReplaceHeader(
            &request->headers,
            nx_http::HttpHeader(
                header::Authorization::NAME,
                basicAuthorization.serialized()));
        return true;
    }
    else if (wwwAuthenticateHeader.authScheme == header::AuthScheme::digest)
    {
        header::DigestAuthorization digestAuthorizationHeader;
        if (!calcDigestResponse(
                request->requestLine.method,
                credentials,
                request->requestLine.url.path().toUtf8(),
                wwwAuthenticateHeader,
                &digestAuthorizationHeader))
        {
            return false;
        }
        insertOrReplaceHeader(
            &request->headers,
            nx_http::HttpHeader(
                header::Authorization::NAME,
                digestAuthorizationHeader.serialized()));
        return true;
    }

    return false;
}

boost::optional<QCryptographicHash::Algorithm> parseAlgorithm(
    const StringType& algorithm)
{
    // TODO: #mux Perhaps we should support some other algorithms?
    if (algorithm == "MD5") return QCryptographicHash::Md5;
    if (algorithm == "SHA-256") return QCryptographicHash::Sha256;

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


static BufferType fieldOrEmpty(
    const QMap<BufferType, BufferType>& inputParams,
    const BufferType& name)
{
    const auto iter = inputParams.find(name);
    return iter != inputParams.end() ? iter.value() : BufferType();
}

static bool calcDigestResponse(
    const QByteArray& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHa1,
    const StringType& uri,
    const QMap<BufferType, BufferType>& inputParams,
    QMap<BufferType, BufferType>* const outputParams)
{
    const auto algorithm = fieldOrEmpty(inputParams, "algorithm");
    if (!parseAlgorithm(algorithm))
        return false; //< such algorithm is not supported

    const auto nonce = fieldOrEmpty(inputParams, "nonce");
    const auto realm = fieldOrEmpty(inputParams, "realm");
    const auto qop = fieldOrEmpty(inputParams, "qop");

    // TODO #ak qop can have value "auth,auth-int". That should be supported
    if (qop.indexOf("auth-int") != -1)
        return false; //< qop=auth-int is not supported

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
        outputParams->insert("algorithm", algorithm);

    outputParams->insert("username", userName);
    outputParams->insert("realm", realm);
    outputParams->insert("nonce", nonce);
    outputParams->insert("uri", uri);

    const BufferType nonceCount = "00000001";     //TODO #ak generate it
    const BufferType clientNonce = "0a4f113b";    //TODO #ak generate it

    QByteArray digestResponse;
    if (qop.isEmpty())
    {
        digestResponse = calcResponse(ha1, nonce, ha2, algorithm);
    }
    else
    {
        digestResponse = calcResponseAuthInt(
            ha1, nonce, nonceCount, clientNonce, qop, ha2, algorithm);

        outputParams->insert("qop", qop);
        outputParams->insert("nc", nonceCount);
        outputParams->insert("cnonce", clientNonce);
    }
    outputParams->insert("response", digestResponse);
    return true;
}

bool calcDigestResponse(
    const QByteArray& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHa1,
    const StringType& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader)
{
    if (wwwAuthenticateHeader.authScheme != header::AuthScheme::digest)
        return false;

    //TODO #ak have to set digestAuthorizationHeader->digest->userid somewhere

    return calcDigestResponse(
        method,
        userName,
        userPassword,
        predefinedHa1,
        uri,
        wwwAuthenticateHeader.params,
        &digestAuthorizationHeader->digest->params);
}

bool calcDigestResponse(
    const StringType& method,
    const Credentials& credentials,
    const StringType& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader)
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
        digestAuthorizationHeader);
}

bool validateAuthorization(
    const StringType& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHa1,
    const header::DigestAuthorization& digestAuthorizationHeader)
{
    const auto& digestParams = digestAuthorizationHeader.digest->params;

    const auto uri = fieldOrEmpty(digestParams, "uri");
    if (uri.isEmpty())
        return false;

    const auto algorithm = fieldOrEmpty(digestParams, "algorithm");
    if (!parseAlgorithm(algorithm))
        return false; //< Such algorithm is not supported.

    QMap<BufferType, BufferType> outputParams;
    const auto result = calcDigestResponse(
        method, userName, userPassword, predefinedHa1, uri,
        digestParams, &outputParams);

    if (!result)
        return false;

    const auto calculatedResponse = fieldOrEmpty(outputParams, "response");
    const auto response = fieldOrEmpty(digestParams, "response");
    return !response.isEmpty() && response == calculatedResponse;
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

} // namespace nx_http
