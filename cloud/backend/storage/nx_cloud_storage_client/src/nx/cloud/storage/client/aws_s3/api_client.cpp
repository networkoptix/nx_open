#include "api_client.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>

namespace nx::cloud::storage::client::aws_s3 {

ApiClient::ApiClient(
    const std::string& /*storageClientId*/,
    const nx::utils::Url& url,
    const nx::network::http::Credentials& /*credentials*/)
    :
    m_url(url)
{
    bindToAioThread(getAioThread());
}

void ApiClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_requestPool.bindToAioThread(aioThread);
}

void ApiClient::setTimeouts(const nx::network::http::AsyncClient::Timeouts& timeouts)
{
    m_timeouts = timeouts;
}

void ApiClient::uploadFile(
    const std::string& destinationPath,
    nx::Buffer data,
    CommonHandler handler)
{
    using namespace nx::network;

    dispatch(
        [this, destinationPath, data = std::move(data), handler = std::move(handler)]() mutable
        {
            auto& context = m_requestPool.add(
                std::make_unique<http::AsyncClient>(),
                [this](auto&&... args) { handleUploadResult(std::forward<decltype(args)>(args)...); },
                std::move(handler));
            context.executor->setTimeouts(m_timeouts);

            const auto url = url::Builder(m_url).appendPath(destinationPath);
            // TODO: #ak Add Authorization.
            context.executor->doPut(
                url,
                std::make_unique<http::BufferSource>("application/octet-stream", std::move(data)),
                [this, &context]() { m_requestPool.completeRequest(&context); });
        });
}

void ApiClient::downloadFile(
    const std::string& path,
    DownloadHandler handler)
{
    using namespace nx::network;

    dispatch(
        [this, path, handler = std::move(handler)]() mutable
        {
            auto& context = m_requestPool.add(
                std::make_unique<http::AsyncClient>(),
                [this](auto&&... args) { handleDownloadResult(std::forward<decltype(args)>(args)...); },
                std::move(handler));
            context.executor->setTimeouts(m_timeouts);

            const auto url = url::Builder(m_url).appendPath(path);
            // TODO: #ak Add Authorization.
            context.executor->doGet(
                url,
                [this, &context]() { m_requestPool.completeRequest(&context); });
        });
}

void ApiClient::stopWhileInAioThread()
{
    m_requestPool.pleaseStopSync();
    // TODO
}

void ApiClient::handleUploadResult(
    std::unique_ptr<nx::network::http::AsyncClient> httpClient,
    CommonHandler userHandler)
{
    userHandler(getResultCode(*httpClient));
}

void ApiClient::handleDownloadResult(
    std::unique_ptr<nx::network::http::AsyncClient> httpClient,
    DownloadHandler userHandler)
{
    const auto resultCode = getResultCode(*httpClient);
    if (resultCode != ResultCode::ok)
        return userHandler(Result(resultCode), nx::Buffer());

    userHandler(resultCode, httpClient->fetchMessageBodyBuffer());
}

ResultCode ApiClient::getResultCode(const nx::network::http::AsyncClient& httpClient) const
{
    if (!httpClient.response())
        return ResultCode::networkError;

    const auto statusCode = httpClient.response()->statusLine.statusCode;

    if (nx::network::http::StatusCode::isSuccessCode(statusCode))
        return ResultCode::ok;

    switch (statusCode)
    {
        case nx::network::http::StatusCode::forbidden:
        case nx::network::http::StatusCode::unauthorized:
            return ResultCode::unauthorized;

        default:
            return ResultCode::error;
    }

    return ResultCode::ok;
}

} // namespace nx::cloud::storage::client::aws_s3
