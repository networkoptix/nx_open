#include "api_client.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include "aws_signature_v4.h"
#include "http_request_paths.h"
#include "api/location_constraint.h"
#include "api/list_bucket_result.h"

namespace nx::cloud::aws {

namespace {

static constexpr char kAwsS3ServiceName[] = "s3";

} // namespace

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
    doAwsApiCall(
        network::http::Method::put,
        destinationPath,
        [this, handler = std::move(handler)](auto httpClient) mutable
        {
            handler(getResultCode(*httpClient));
        },
        std::make_unique<network::http::BufferSource>(
            "application/octet-stream",
            std::move(data)));
}

void ApiClient::downloadFile(
    const std::string& path,
    DownloadHandler handler)
{
    doAwsApiCall(
        nx::network::http::Method::get,
        path,
        [this, handler = std::move(handler)](auto httpClient) mutable
        {
            const auto resultCode = getResultCode(*httpClient);
            return handler(Result(resultCode), httpClient->fetchMessageBodyBuffer());
        });
}

void ApiClient::deleteFile(
    const std::string& path,
    CommonHandler handler)
{
    doAwsApiCall(
        nx::network::http::Method::delete_,
        path,
        [this, handler = std::move(handler)](auto httpClient) mutable
        {
            handler(getResultCode(*httpClient));
        });
}

void ApiClient::getLocation(
    nx::utils::MoveOnlyFunc<void(Result, std::string)> handler)
{
    auto url =
        nx::network::url::Builder(m_url).setPath(http::kRootPath).setQuery(http::kLocationQuery);
    doAwsApiCall(
        nx::network::http::Method::get,
        url,
        [this, handler = std::move(handler)](auto httpClient)
        {
            api::LocationConstraint locationConstraint;
            auto messageBody = httpClient->fetchMessageBodyBuffer();
            if (!api::xml::deserialize(messageBody, &locationConstraint))
            {
                return handler(
                    Result(getResultCode(*httpClient), messageBody.constData()),
                    std::string());
            }

            handler(ResultCode::ok, std::move(locationConstraint.region));
        });
}

void nx::cloud::aws::ApiClient::getBucketSize(
    nx::utils::MoveOnlyFunc<void(Result, int)> handler)
{
    QString prefix;
    auto path = m_url.path();
    if (!path.isEmpty())
        prefix = QString("&prefix=") + (path[0] == '/' ? path.mid(1) : path);

    auto url =
        nx::network::url::Builder(m_url).setPath(http::kRootPath)
            .setQuery("list_type=2" + prefix);

    doAwsApiCall(
        nx::network::http::Method::get,
        url,
        [this, handler = std::move(handler)](auto httpClient)
        {
            auto resultCode = getResultCode(*httpClient);
            if (resultCode != ResultCode::ok)
            {
                return handler(
                    Result(resultCode, httpClient->fetchMessageBodyBuffer().constData()),
                    0);
            }
            api::ListBucketResult listBucketResult;
            if (!api::xml::deserialize(httpClient->fetchMessageBodyBuffer(), &listBucketResult))
            {
                return handler(
                    Result(ResultCode::error, "Failed to deserialize ListBucketResult"),
                    0);
            }

            int size = 0;
            for(const auto& object: listBucketResult.contents)
                size += object.size;

            handler(ResultCode::ok, size);
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

template<typename Handler>
void ApiClient::doAwsApiCall(
    const nx::network::http::Method::ValueType& method,
    const std::string& path,
    Handler handler,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> body)
{
    doAwsApiCall(
        method,
        nx::network::url::Builder(m_url).appendPath(path),
        std::move(handler),
        std::move(body));
}

template<typename Handler>
void ApiClient::doAwsApiCall(
    const nx::network::http::Method::ValueType& method,
    const nx::utils::Url& url,
    Handler handler,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> body)
{
    using namespace nx::network;

    dispatch(
        [this, method, url, handler = std::move(handler), body = std::move(body)]() mutable
    {
        auto& context = m_requestPool.add(
            prepareHttpClient(),
            std::move(handler));
        context.executor->setTimeouts(m_timeouts);

        if (body)
        {
            body->bindToAioThread(getAioThread());
            context.executor->setRequestBody(std::move(body));
        }

        context.executor->doRequest(
            method,
            url,
            [this, &context]() { m_requestPool.completeRequest(&context); });
    });
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

} // namespace nx::cloud::aws
