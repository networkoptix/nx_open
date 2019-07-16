#include "aws_s3_emulator.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/aws/aws_signature_v4.h>

namespace nx::cloud::aws::test {

AwsS3Emulator::AwsS3Emulator()
{
    registerHttpApi();
}

bool AwsS3Emulator::bindAndListen(const nx::network::SocketAddress& endpoint)
{
    return m_httpServer.bindAndListen(endpoint);
}

nx::network::SocketAddress AwsS3Emulator::serverAddress() const
{
    return m_httpServer.serverAddress();
}

void AwsS3Emulator::enableAthentication(
    const std::regex& path,
    nx::network::http::Credentials credentials)
{
    m_httpServer.authDispatcher().add(path, &m_awsAuthenticator);
    m_awsAuthenticator.addCredentials(
        credentials.username.toStdString(),
        credentials.authToken.value.toStdString());
}

nx::utils::Url AwsS3Emulator::baseApiUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_httpServer.serverAddress());
}

void AwsS3Emulator::saveOrReplaceFile(
    const std::string& path,
    nx::Buffer contents)
{
    QnMutexLocker lock(&m_mutex);

    m_pathToFileContents[path] = std::move(contents);
}

std::optional<nx::Buffer> AwsS3Emulator::getFile(const std::string& path) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_pathToFileContents.find(path); it != m_pathToFileContents.end())
        return it->second;

    return std::nullopt;
}

void AwsS3Emulator::registerHttpApi()
{
    // TODO: #ak Authentication.

    m_httpServer.registerRequestProcessorFunc(
        nx::network::http::kAnyPath,
        [this](auto&&... args) { saveFile(std::forward<decltype(args)>(args)...); },
        nx::network::http::Method::put);

    m_httpServer.registerRequestProcessorFunc(
        nx::network::http::kAnyPath,
        [this](auto&& ... args) { getFile(std::forward<decltype(args)>(args)...); },
        nx::network::http::Method::get);
}

void AwsS3Emulator::saveFile(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    // TODO: #ak Check Content-MD5 if present.

    saveOrReplaceFile(
        requestContext.request.requestLine.url.path().toStdString(),
        requestContext.request.messageBody);

    completionHandler(nx::network::http::StatusCode::ok);
}

void AwsS3Emulator::getFile(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto fileContents = getFile(requestContext.request.requestLine.url.path().toStdString());
    if (!fileContents)
        return completionHandler(nx::network::http::StatusCode::notFound);

    completionHandler(network::http::RequestResult{
        nx::network::http::StatusCode::ok,
        std::make_unique<nx::network::http::BufferSource>(
            "application/octet-stream", *fileContents),
        {}});
}

//-------------------------------------------------------------------------------------------------

void AwsSignatureV4Authenticator::authenticate(
    const nx::network::http::HttpServerConnection& /*connection*/,
    const nx::network::http::Request& request,
    nx::network::http::server::AuthenticationCompletionHandler completionHandler)
{
    auto authorizationIt =
        request.headers.find(nx::network::http::header::Authorization::NAME);
    if (authorizationIt == request.headers.end())
        return completionHandler(prepareUnsuccesfulResult());

    const auto authorization = authorizationIt->second;
    if (!authorization.startsWith("AWS4-HMAC-SHA256 "))
        return completionHandler(prepareUnsuccesfulResult());

    std::map<nx::String, nx::String> params;
    for (const auto& token: authorization.mid(sizeof("AWS4-HMAC-SHA256 ") - 1).split(','))
    {
        const auto tokens = token.split('=');
        if (tokens.empty())
            continue;
        params.emplace(tokens[0], tokens.size() > 1 ? tokens[1] : nx::String());
    }

    const auto credentialIt = params.find("Credential");
    if (credentialIt == params.end())
        return completionHandler(prepareUnsuccesfulResult());

    const auto tokens = credentialIt->second.split('/');
    const auto accessKeyId = tokens[0];
    const auto region = tokens[2];
    const auto service = tokens[3];

    const auto accessKeyIter = m_credentials.find(accessKeyId.toStdString());
    if (accessKeyIter == m_credentials.end())
        return completionHandler(prepareUnsuccesfulResult());

    const auto signatureIt = params.find("Signature");
    if (signatureIt == params.end())
        return completionHandler(prepareUnsuccesfulResult());
    const auto signature = signatureIt->second;

    const auto [calculatedSignature, result] = SignatureCalculator::calculateSignature(
        request,
        nx::network::http::Credentials(
            accessKeyId,
            nx::network::http::PasswordAuthToken(accessKeyIter->second.c_str())),
        region.toStdString(),
        service.toStdString());

    if (!result)
        return completionHandler(prepareUnsuccesfulResult()); //< Request malformed.

    if (calculatedSignature != signature)
        return completionHandler(prepareUnsuccesfulResult());

    completionHandler(nx::network::http::server::SuccessfulAuthenticationResult());
}

void AwsSignatureV4Authenticator::addCredentials(
    const std::string& accessKeyId,
    const std::string& secretAccessKey)
{
    m_credentials[accessKeyId] = secretAccessKey;
}

nx::network::http::server::AuthenticationResult
    AwsSignatureV4Authenticator::prepareUnsuccesfulResult()
{
    nx::network::http::server::AuthenticationResult authenticationResult;
    authenticationResult.isSucceeded = false;
    // authenticationResult.msgBody = std::make
    return authenticationResult;
}

} // namespace nx::cloud::aws::test
