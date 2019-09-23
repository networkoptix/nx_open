#include "base_api_client.h"

#include "aws_signature_v4.h"

namespace nx::cloud::aws {

BaseApiClient::BaseApiClient(
    const std::string& service,
    const std::string& awsRegion,
    const nx::utils::Url& url,
    const Credentials& credentials)
    :
    m_service(service),
    m_awsRegion(awsRegion),
    m_url(url),
    m_credentials(credentials)
{
    bindToAioThread(getAioThread());
}

BaseApiClient::~BaseApiClient()
{
    pleaseStopSync();
}

void BaseApiClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_requestPool.bindToAioThread(aioThread);
}

void BaseApiClient::setTimeouts(const nx::network::http::AsyncClient::Timeouts& timeouts)
{
    m_timeouts = timeouts;
}

void BaseApiClient::stopWhileInAioThread()
{
    m_requestPool.pleaseStopSync();
}

std::unique_ptr<network::http::AsyncClient> BaseApiClient::prepareHttpClient()
{
    auto client = std::make_unique<network::http::AsyncClient>();
    client->setCustomRequestPrepareFunc(
        [this](auto* request) { addAuthorizationToRequest(request); });
    client->setTimeouts(m_timeouts);
    return client;
}

void BaseApiClient::addAuthorizationToRequest(network::http::Request* request)
{
    static constexpr char kHexSha256OfEmptyString[] =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    static constexpr char kUnsignedPayloadHash[] = "UNSIGNED-PAYLOAD";

    // x-amz-content-sha256
    if (request->requestLine.method == network::http::Method::get)
        request->headers.emplace("x-amz-content-sha256", kHexSha256OfEmptyString);
    else
        request->headers.emplace("x-amz-content-sha256", kUnsignedPayloadHash);

    if (!m_credentials.sessionToken.isEmpty())
        request->headers.emplace("x-amz-security-token", m_credentials.sessionToken);

    // x-amz-date
    auto iso8601Date = QDateTime::currentDateTime().toUTC().toString(Qt::ISODate).toUtf8();
    iso8601Date.replace("-", "");
    iso8601Date.replace(":", "");
    request->headers.emplace("x-amz-date", iso8601Date);

    if (!m_credentials.username.isEmpty())
    {
        const auto [authorization, result] = calculateAuthorizationHeader(
            *request,
            m_credentials,
            m_awsRegion,
            m_service);
        NX_ASSERT(result);

        request->headers.emplace(network::http::header::Authorization::NAME, authorization);
    }
}

std::tuple<nx::String, bool> BaseApiClient::calculateAuthorizationHeader(
    const network::http::Request& request,
    const Credentials& credentials,
    const std::string& region,
    const std::string& service)
{
    return SignatureCalculator().calculateAuthorizationHeader(
        request,
        credentials,
        region,
        service);
}

ResultCode BaseApiClient::getResultCode(const nx::network::http::AsyncClient& httpClient) const
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