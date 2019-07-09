#include "aws_s3_emulator.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>

namespace nx::cloud::storage::client::aws_s3::test {

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

} // namespace nx::cloud::storage::client::aws_s3::test
