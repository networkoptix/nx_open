#include "api_client.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include "aws_signature_v4.h"

namespace nx::cloud::storage::client::aws_s3 {

static constexpr char kAwsS3ServiceName[] = "s3";

ApiClient::ApiClient(
    const std::string& /*storageClientId*/,
    const std::string& awsRegion,
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials)
    :
    m_awsRegion(awsRegion),
    m_url(url),
    m_credentials(credentials)
{
    bindToAioThread(getAioThread());
}

ApiClient::~ApiClient()
{
    pleaseStopSync();
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
                prepareHttpClient(),
                [this](auto&&... args) { handleUploadResult(std::forward<decltype(args)>(args)...); },
                std::move(handler));
            context.executor->setTimeouts(m_timeouts);

            const auto url = url::Builder(m_url).appendPath(destinationPath);
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
                prepareHttpClient(),
                [this](auto&&... args) { handleDownloadResult(std::forward<decltype(args)>(args)...); },
                std::move(handler));
            context.executor->setTimeouts(m_timeouts);

            const auto url = url::Builder(m_url).appendPath(path);
            context.executor->doGet(
                url,
                [this, &context]() { m_requestPool.completeRequest(&context); });
        });
}

void ApiClient::stopWhileInAioThread()
{
    m_requestPool.pleaseStopSync();
}

std::unique_ptr<network::http::AsyncClient> ApiClient::prepareHttpClient()
{
    auto client = std::make_unique<network::http::AsyncClient>();
    client->setCustomRequestPrepareFunc(
        [this](auto* request) { addAuthorizationToRequest(request); });
    return client;
}

void ApiClient::addAuthorizationToRequest(network::http::Request* request)
{
    static constexpr char kHexSha256OfEmptyString[] =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    static constexpr char kUnsignedPayloadHash[] = "UNSIGNED-PAYLOAD";

    // x-amz-content-sha256
    if (request->requestLine.method == network::http::Method::get)
        request->headers.emplace("x-amz-content-sha256", kHexSha256OfEmptyString);
    else
        request->headers.emplace("x-amz-content-sha256", kUnsignedPayloadHash);

    // x-amz-date
    auto iso8601Date = QDateTime::currentDateTime().toUTC().toString(Qt::ISODate).toUtf8();
    iso8601Date.replace("-", "");
    iso8601Date.replace(":", "");
    request->headers.emplace("x-amz-date", iso8601Date);

    if (!m_credentials.username.isEmpty())
    {
        const auto [authorization, result] = SignatureCalculator::calculateAuthorizationHeader(
            *request,
            m_credentials,
            m_awsRegion,
            kAwsS3ServiceName);
        NX_ASSERT(result);

        request->headers.emplace(network::http::header::Authorization::NAME, authorization);
    }
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
    return userHandler(Result(resultCode), httpClient->fetchMessageBodyBuffer());
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
