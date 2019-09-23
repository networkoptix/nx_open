#include "aws_s3_emulator.h"

#include <QXmlStreamWriter>

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>

#include <nx/cloud/aws/s3/aws_signature_v4.h>
#include <nx/cloud/aws/http_request_paths.h>
#include <nx/cloud/aws/s3/http_request_paths.h>
#include <nx/cloud/aws/s3/location_constraint.h>

namespace nx::cloud::aws::s3::test {

namespace {

std::string calculateUpperBound(const std::string& keyPrefix)
{
    if (!keyPrefix.empty())
    {
        std::string upperBound = keyPrefix;
        for (auto rit = upperBound.rbegin(); rit != upperBound.rend(); ++rit)
        {
            if (++(*rit) != 0)
                return upperBound;
        }
    }
    return {};
}

std::string parseFileName(const std::string& path, const std::string& prefix)
{
    if (path.length() <= prefix.length())
        return {};
    return path.substr(prefix.length());
}

} // namespace

std::string randomS3Location()
{
    return nx::utils::random::choice(kS3Locations);
}

AwsS3Emulator::AwsS3Emulator(std::string location):
    m_location(std::move(location))
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
        aws::http::kRootPath,
        [this](auto&& ... args)
        { dispatchRootPathGetRequest(std::forward<decltype(args)>(args)...); },
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
    if (requestContext.request.requestLine.url.query() != http::kLocation)
        return completionHandler(network::http::StatusCode::badRequest);

    auto buffer = xml::serialized({this->location()});

    network::http::RequestResult result(network::http::StatusCode::ok);
    result.dataSource =
        std::make_unique<network::http::BufferSource>(
            "application/xml",
            std::move(buffer));
    completionHandler(std::move(result));

}

void AwsS3Emulator::listBucket(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    auto buffer = xml::serialized(
        getListBucketResult(
            parseQuery(
                requestContext.request.requestLine.url.query())));

    network::http::RequestResult result(network::http::StatusCode::ok);
    result.dataSource =
        std::make_unique<network::http::BufferSource>(
            "application/xml",
            std::move(buffer));
    completionHandler(std::move(result));
}

void AwsS3Emulator::dispatchRootPathGetRequest(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (requestContext.request.requestLine.url.query() == http::kLocation)
        return getLocation(std::move(requestContext), std::move(completionHandler));
    listBucket(std::move(requestContext), std::move(completionHandler));
}

ListBucketResult AwsS3Emulator::getListBucketResult(
    std::map<QString, QString> queries) const
{
    const auto prefix = queries[http::kPrefix].toStdString();

    ListBucketResult result;
    result.prefix = prefix;

    QnMutexLocker lock(&m_mutex);
    auto itLow = m_pathToFileContents.begin();
    if (!prefix.empty())
        itLow = m_pathToFileContents.lower_bound(prefix);

    auto itHigh = m_pathToFileContents.end();
    auto upper = calculateUpperBound(prefix);
    if (!upper.empty())
        itHigh = m_pathToFileContents.upper_bound(upper);

    for (auto it = itLow; it != itHigh; ++it)
    {
        Contents contents;
        contents.key = !prefix.empty() ? parseFileName(it->first, prefix) : it->first;
        contents.size = it->second.size();
        result.contents.emplace_back(std::move(contents));
    }

    result.keyCount = (int) result.contents.size();
    result.maxKeys = result.keyCount;

    return result;
}

std::map<QString, QString> AwsS3Emulator::parseQuery(const QString& query) const
{
    std::map<QString, QString> result;
    auto tokens = query.split("&");
    for (const auto& token : tokens)
    {
        int index = token.indexOf("=");
        if (index != -1)
            result.emplace(token.left(index - 1), token.mid(index + 1));
    }
    return result;
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

    s3::SignatureCalculator calculator;
    const auto [calculatedSignature, result] = calculator.calculateSignature(
        request,
        Credentials(
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

} // namespace nx::cloud::aws::s3::test
