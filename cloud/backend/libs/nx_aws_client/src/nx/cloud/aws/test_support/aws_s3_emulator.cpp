#include "aws_s3_emulator.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>

#include <nx/cloud/aws/aws_signature_v4.h>
#include <nx/cloud/aws/http_request_paths.h>

namespace nx::cloud::aws::test {

namespace {

static constexpr char kLocationTemplate[] = R"xml(
    <?xml version = "1.0" encoding = "UTF-8"?>
    <LocationConstraint xmlns="http://s3.amazonaws.com/doc/2006-03-01/">%1</LocationConstraint>
)xml";

static constexpr char kDefaultLocationTemplate[] = R"xml(
    <LocationConstraint xmlns="http://s3.amazonaws.com/doc/2006-03-01/"/>
)xml";

} // namespace

std::string randomS3Location()
{
    return nx::utils::random::choice(kS3Locations);
}

AwsS3Emulator::AwsS3Emulator(std::string location):
    m_location(location)
{
    registerHttpApi();
}

bool AwsS3Emulator::bindAndListen(const network::SocketAddress& endpoint)
{
    return m_httpServer.bindAndListen(endpoint);
}

network::SocketAddress AwsS3Emulator::serverAddress() const
{
    return m_httpServer.serverAddress();
}

void AwsS3Emulator::enableAthentication(
    const std::regex& path,
    network::http::Credentials credentials)
{
    m_httpServer.authDispatcher().add(path, &m_awsAuthenticator);
    m_awsAuthenticator.addCredentials(
        credentials.username.toStdString(),
        credentials.authToken.value.toStdString());
}

nx::utils::Url AwsS3Emulator::baseApiUrl() const
{
    return network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
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

bool AwsS3Emulator::deleteFile(const std::string& path)
{
    QnMutexLocker lock(&m_mutex);

    return m_pathToFileContents.erase(path) > 0;
}

std::string AwsS3Emulator::location() const
{
    QnMutexLocker lock(&m_mutex);

    return m_location;
}

void AwsS3Emulator::setLocation(const std::string& location)
{
    QnMutexLocker lock(&m_mutex);

    m_location = location;
}

void AwsS3Emulator::registerHttpApi()
{
    m_httpServer.registerRequestProcessorFunc(
        network::http::kAnyPath,
        [this](auto&&... args) { saveFile(std::forward<decltype(args)>(args)...); },
        network::http::Method::put);

    m_httpServer.registerRequestProcessorFunc(
        network::http::kAnyPath,
        [this](auto&& ... args) { getFile(std::forward<decltype(args)>(args)...); },
        network::http::Method::get);

    m_httpServer.registerRequestProcessorFunc(
        network::http::kAnyPath,
        [this](auto&& ... args) { deleteFile(std::forward<decltype(args)>(args)...); },
        network::http::Method::delete_);

    m_httpServer.registerRequestProcessorFunc(
        /*aws::http::kRootPath,*/
        "/",
        [this](auto&& ... args) { getLocation(std::forward<decltype(args)>(args)...); },
        network::http::Method::get);
}

void AwsS3Emulator::saveFile(
    network::http::RequestContext requestContext,
    network::http::RequestProcessedHandler completionHandler)
{
    // TODO: #ak Check Content-MD5 if present.

    saveOrReplaceFile(
        requestContext.request.requestLine.url.path().toStdString(),
        requestContext.request.messageBody);

    completionHandler(network::http::StatusCode::ok);
}

void AwsS3Emulator::getFile(
    network::http::RequestContext requestContext,
    network::http::RequestProcessedHandler completionHandler)
{
    const auto fileContents = getFile(requestContext.request.requestLine.url.path().toStdString());
    if (!fileContents)
        return completionHandler(network::http::StatusCode::notFound);

    completionHandler(network::http::RequestResult{
        network::http::StatusCode::ok,
        std::make_unique<network::http::BufferSource>(
            "application/octet-stream", *fileContents),
        {}});
}

void AwsS3Emulator::deleteFile(
    network::http::RequestContext requestContext,
    network::http::RequestProcessedHandler completionHandler)
{
    const auto deleted = deleteFile(requestContext.request.requestLine.url.path().toStdString());
    if (!deleted)
        return completionHandler(network::http::StatusCode::notFound);

    completionHandler(network::http::StatusCode::noContent);
}

void AwsS3Emulator::getLocation(
    network::http::RequestContext requestContext,
    network::http::RequestProcessedHandler completionHandler)
{
    if (requestContext.request.requestLine.url.query() != http::kLocationQuery)
        return completionHandler(network::http::StatusCode::badRequest);

    auto location = this->location();

    auto buffer =
        location == kDefaultS3Location
        ? nx::Buffer(kDefaultLocationTemplate)
        : lm(kLocationTemplate).arg(location).toUtf8();

    network::http::RequestResult result(network::http::StatusCode::ok);
    result.dataSource =
        std::make_unique<network::http::BufferSource>(
            "application/xml",
            std::move(buffer));
    completionHandler(std::move(result));

}

//-------------------------------------------------------------------------------------------------

void AwsSignatureV4Authenticator::authenticate(
    const network::http::HttpServerConnection& /*connection*/,
    const network::http::Request& request,
    network::http::server::AuthenticationCompletionHandler completionHandler)
{
    auto authorizationIt =
        request.headers.find(network::http::header::Authorization::NAME);
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
        network::http::Credentials(
            accessKeyId,
            network::http::PasswordAuthToken(accessKeyIter->second.c_str())),
        region.toStdString(),
        service.toStdString());

    if (!result)
        return completionHandler(prepareUnsuccesfulResult()); //< Request malformed.

    if (calculatedSignature != signature)
        return completionHandler(prepareUnsuccesfulResult());

    completionHandler(network::http::server::SuccessfulAuthenticationResult());
}

void AwsSignatureV4Authenticator::addCredentials(
    const std::string& accessKeyId,
    const std::string& secretAccessKey)
{
    m_credentials[accessKeyId] = secretAccessKey;
}

network::http::server::AuthenticationResult
    AwsSignatureV4Authenticator::prepareUnsuccesfulResult()
{
    network::http::server::AuthenticationResult authenticationResult;
    authenticationResult.isSucceeded = false;
    // authenticationResult.msgBody = std::make
    return authenticationResult;
}

} // namespace nx::cloud::aws::test
