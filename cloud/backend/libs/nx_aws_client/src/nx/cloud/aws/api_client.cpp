#include "api_client.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include "aws_signature_v4.h"
#include "http_request_paths.h"

namespace nx::cloud::aws {

namespace {

static constexpr char kAwsS3ServiceName[] = "s3";

static std::string parseLocationConstraintResponse(const nx::Buffer& messageBody)
{
    /**
     * Parses aws-region-x between tags <LocationConstraint>aws-region-x</LocationRestraint>
     * if </LocationConstraint> end tag is missing, the region is assumed to be us-east-1 per
     * AWS docs: https://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGETlocation.html
     */

    static constexpr char kLocationConstraint[] = "LocationConstraint";
    if (messageBody.indexOf(kLocationConstraint) == -1) //< Required xml tag is missing
        return {};

    static constexpr char kXmlEndTag[] = "</LocationConstraint>";
    static constexpr char kUsEast1[] = "us-east-1";

    int end = messageBody.indexOf(kXmlEndTag);
    if (end == -1)
        return kUsEast1;

    for (int i = end - 1; i >= 0; --i)
    {
        if (messageBody.at(i) == '>')
            return messageBody.mid(i + 1, end - i - 1).trimmed();
    }

    return {};
}

static std::string parseWrongRegionResponse(const nx::Buffer& messageBody)
{
    // Parses 'aws-region-x' between tags: <Region>aws-region-x</Region>
    static QString kRegionStartTag = "<Region>";
    static QString kRegionEndTag = "</Region>";

    int start = messageBody.indexOf(kRegionStartTag);
    if (start == -1)
        return {};
    start += kRegionStartTag.size();

    int end = messageBody.indexOf(kRegionEndTag, start);
    if (end == -1)
        return {};

    if (end <= start)
        return {};

    return messageBody.mid(start, end - start).constData();
}

static std::vector<std::function<std::string(const nx::Buffer&)>> kLocationXmlParsers = {
    parseWrongRegionResponse,
    parseLocationConstraintResponse
};

// TODO figure out xml deserialization AND how to deal with
// aws authorization catch 22 for getLocation api call
static std::string parseLocation(const nx::Buffer& messageBody)
{
    if (messageBody.isEmpty())
        return {};

    for (auto& func : kLocationXmlParsers)
    {
        auto region = func(messageBody);
        if (!region.empty())
            return region;
    }
    return {};
}

}

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
    // TODO fix this entire api call.
    auto url =
        nx::network::url::Builder(m_url).setPath(http::kRootPath).setQuery(http::kLocationQuery);
    doAwsApiCall(
        nx::network::http::Method::get,
        url,
        [this, handler = std::move(handler)](auto httpClient) mutable
        {
            auto messageBody = httpClient->fetchMessageBodyBuffer();
            std::string location = parseLocation(messageBody);
            if (!location.empty())
            {
                handler(ResultCode::ok, std::move(location));
            }
            else
            {
                handler(
                    Result(getResultCode(*httpClient), messageBody.constData()),
                    std::string());
            }
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
