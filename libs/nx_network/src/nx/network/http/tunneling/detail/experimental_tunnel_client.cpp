#include "experimental_tunnel_client.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/uuid.h>

#include "request_paths.h"
#include "separate_up_down_channel_delegate.h"
#include "../../rest/http_rest_client.h"

namespace nx::network::http::tunneling::detail {

ExperimentalTunnelClient::ExperimentalTunnelClient(
    const nx::utils::Url& baseTunnelUrl,
    ClientFeedbackFunction clientFeedbackFunction)
    :
    base_type(baseTunnelUrl, std::move(clientFeedbackFunction))
{
}

void ExperimentalTunnelClient::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_downChannelHttpClient)
        m_downChannelHttpClient->bindToAioThread(aioThread);
    if (m_upChannelHttpClient)
        m_upChannelHttpClient->bindToAioThread(aioThread);

    if (m_downChannel)
        m_downChannel->bindToAioThread(aioThread);
    if (m_upChannel)
        m_upChannel->bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
}

void ExperimentalTunnelClient::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void ExperimentalTunnelClient::openTunnel(
    OpenTunnelCompletionHandler completionHandler)
{
    m_tunnelId = QnUuid::createUuid().toSimpleByteArray().toStdString();
    m_completionHandler = std::move(completionHandler);

    post(
        [this]()
        {
            if (m_timeout && *m_timeout != kNoTimeout)
            {
                m_timer.start(
                    *m_timeout,
                    [this]() { handleTunnelFailure(SystemError::timedOut); });
            }

            initiateDownChannel();
            initiateUpChannel();
        });
}

void ExperimentalTunnelClient::initiateDownChannel()
{
    m_downChannelHttpClient = std::make_unique<AsyncClient>();
    m_downChannelHttpClient->setAdditionalHeaders(customHeaders());
    if (m_timeout)
    {
        m_downChannelHttpClient->setResponseReadTimeout(*m_timeout);
        m_downChannelHttpClient->setMessageBodyReadTimeout(*m_timeout);
    }

    initiateChannel(
        m_downChannelHttpClient.get(),
        Method::get,
        kExperimentalTunnelDownPath,
        [this]() { onDownChannelOpened(); });
}

void ExperimentalTunnelClient::onDownChannelOpened()
{
    if (!m_downChannelHttpClient->hasRequestSucceeded())
        return cleanUpFailedTunnel(m_downChannelHttpClient.get());

    m_downChannel = m_downChannelHttpClient->takeSocket();
    m_openTunnelResponse = std::move(*m_downChannelHttpClient->response());
    m_downChannelHttpClient.reset();

    reportTunnelIfReady();
}

void ExperimentalTunnelClient::initiateUpChannel()
{
    m_upChannelHttpClient = std::make_unique<AsyncClient>();
    if (m_timeout)
    {
        m_upChannelHttpClient->setResponseReadTimeout(*m_timeout);
        m_upChannelHttpClient->setMessageBodyReadTimeout(*m_timeout);
    }

    prepareOpenUpChannelRequest();

    initiateChannel(
        m_upChannelHttpClient.get(),
        Method::post,
        kExperimentalTunnelUpPath,
        [this]() { onUpChannelOpened(); });
}

void ExperimentalTunnelClient::onUpChannelOpened()
{
    if (!m_upChannelHttpClient->hasRequestSucceeded())
        return cleanUpFailedTunnel(m_upChannelHttpClient.get());

    m_upChannel = m_upChannelHttpClient->takeSocket();
    m_upChannelHttpClient.reset();

    reportTunnelIfReady();
}

void ExperimentalTunnelClient::initiateChannel(
    AsyncClient* httpClient,
    http::Method::ValueType httpMethod,
    const std::string& requestPath,
    std::function<void()> requestHandler)
{
    httpClient->setResponseReadTimeout(kNoTimeout);
    httpClient->setMessageBodyReadTimeout(kNoTimeout);

    const auto tunnelUrl = url::Builder(m_baseTunnelUrl).appendPath(
        http::rest::substituteParameters(
            requestPath,
            { m_tunnelId }).c_str());

    httpClient->setOnResponseReceived(
        [requestHandler = std::move(requestHandler)]() { requestHandler(); });

    httpClient->doRequest(
        httpMethod,
        tunnelUrl,
        [this, httpClient]() { handleTunnelFailure(httpClient); });
}

const Response& ExperimentalTunnelClient::response() const
{
    return m_openTunnelResponse;
}

void ExperimentalTunnelClient::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    clear();
}

void ExperimentalTunnelClient::clear()
{
    m_downChannelHttpClient.reset();
    m_upChannelHttpClient.reset();

    m_downChannel.reset();
    m_upChannel.reset();

    m_timer.pleaseStopSync();
}

void ExperimentalTunnelClient::handleTunnelFailure(
    AsyncClient* failedHttpClientPtr)
{
    // TODO: #ak Remove if and pass std::unique_ptr<AsyncClient> to this method.
    std::unique_ptr<AsyncClient> failedHttpClient;
    if (failedHttpClientPtr == m_downChannelHttpClient.get())
        failedHttpClient = std::exchange(m_downChannelHttpClient, nullptr);
    else
        failedHttpClient = std::exchange(m_upChannelHttpClient, nullptr);

    clear();

    cleanUpFailedTunnel(failedHttpClient.get());
}

void ExperimentalTunnelClient::handleTunnelFailure(
    SystemError::ErrorCode systemErrorCode)
{
    clear();
    cleanUpFailedTunnel(systemErrorCode);
}

void ExperimentalTunnelClient::prepareOpenUpChannelRequest()
{
    HttpHeaders headers;

    headers.emplace("Content-Type", "application/octet-stream");
    // TODO: #ak HttpServerConnection currently does not support
    // streaming request body. So, it notifies handler only when
    // the whole message has been received.
    //headers.emplace("Content-Length", "10000000000");
    headers.emplace("Content-Length", "0");
    headers.emplace("Pragma", "no-cache");
    headers.emplace("Cache-Control", "no-cache");
    headers.emplace("Connection", "keep-alive");

    m_upChannelHttpClient->setAdditionalHeaders(headers);
}

void ExperimentalTunnelClient::reportTunnelIfReady()
{
    if (!m_downChannel || !m_upChannel)
        return;

    m_connection = std::make_unique<SeparateUpDownChannelDelegate>(
        std::exchange(m_downChannel, nullptr),
        std::exchange(m_upChannel, nullptr));

    m_timer.pleaseStopSync();

    if (!resetConnectionAttributes())
        return reportFailure(OpenTunnelResult(SystemError::getLastOSErrorCode()));

    reportSuccess();
}

} // namespace nx::network::http::tunneling::detail
