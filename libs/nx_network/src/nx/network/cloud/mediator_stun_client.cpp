#include "mediator_stun_client.h"

#include <nx/network/stun/client_connection_validator.h>
#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/network/url/url_builder.h>

#include "mediator_endpoint_provider.h"
#include "mediator/api/mediator_api_http_paths.h"

namespace nx::hpm::api {

MediatorStunClient::MediatorStunClient(
    AbstractAsyncClient::Settings settings,
    MediatorEndpointProvider* endpointProvider)
    :
    base_type(
        [settings]() mutable
        {
            using namespace nx::network::stun;

            settings.reconnectPolicy = nx::network::RetryPolicy::kNoRetries;
            auto client = std::make_unique<AsyncClientWithHttpTunneling>(settings);
            client->setTunnelValidatorFactory([](auto connection, const auto& /*response*/) {
                return std::make_unique<ClientConnectionValidator>(std::move(connection)); });
            return client;
        }()),
    m_endpointProvider(endpointProvider),
    m_reconnectTimer(settings.reconnectPolicy)
{
    bindToAioThread(m_endpointProvider ? m_endpointProvider->getAioThread() : getAioThread());

    base_type::setOnConnectionClosedHandler(
        [this](auto&&... args) { handleConnectionClosure(std::forward<decltype(args)>(args)...); });
}

void MediatorStunClient::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_alivenessTester)
        m_alivenessTester->bindToAioThread(aioThread);
    m_reconnectTimer.bindToAioThread(aioThread);
}

void MediatorStunClient::connect(
    const nx::utils::Url& url,
    ConnectHandler handler)
{
    dispatch(
        [this, url, handler = std::move(handler)]() mutable
        {
            m_url = url;
            m_externallyProvidedUrl = true;

            connectInternal(std::move(handler));
        });
}

void MediatorStunClient::sendRequest(
    nx::network::stun::Message request,
    RequestHandler handler,
    void* client)
{
    dispatch(
        [this, request = std::move(request),
            handler = std::move(handler), client]() mutable
        {
            if (m_url)
            {
                base_type::sendRequest(std::move(request), std::move(handler), client);
                return;
            }

            m_postponedRequests.push_back(
                { std::move(request), std::move(handler), client });

            cancelReconnectTimer();
            connectWithResolving();
        });
}

void MediatorStunClient::setOnConnectionClosedHandler(
    OnConnectionClosedHandler handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            m_onConnectionClosedHandler = std::move(handler);
        });
}

void MediatorStunClient::setKeepAliveOptions(
    nx::network::KeepAliveOptions options)
{
    // NOTE: Intentionally not forwarding call to the base_type.
    m_keepAliveOptions = std::move(options);

    if (m_connected)
        dispatch([this]() { startKeepAliveProbing(); });
}

void MediatorStunClient::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_reconnectTimer.pleaseStopSync();
    m_alivenessTester.reset();
}

void MediatorStunClient::handleConnectionClosure(SystemError::ErrorCode reason)
{
    stopKeepAliveProbing();

    scheduleReconnect();

    if (m_onConnectionClosedHandler)
        m_onConnectionClosedHandler(reason);
}

void MediatorStunClient::connectInternal(ConnectHandler handler)
{
    cancelReconnectTimer();

    base_type::connect(
        *m_url,
        [this, handler = std::move(handler)](auto&&... args) mutable
        {
            handleConnectCompletion(
                std::move(handler),
                std::forward<decltype(args)>(args)...);
        });
}

void MediatorStunClient::scheduleReconnect()
{
    NX_ASSERT(isInSelfAioThread());

    if (m_reconnectTimer.scheduleNextTry([this]() { reconnect(); }))
    {
        NX_VERBOSE(this, "Scheduled reconnect attempt");
    }
    else
    {
        NX_DEBUG(this, "Stopping reconnect attempts");
    }
}

void MediatorStunClient::cancelReconnectTimer()
{
    m_reconnectTimer.cancelSync();
}

void MediatorStunClient::reconnect()
{
    if (m_endpointProvider && !(m_url && m_externallyProvidedUrl))
    {
        NX_VERBOSE(this, "Reconnecting via resolve");
        m_url = std::nullopt;
        connectWithResolving();
    }
    else
    {
        NX_ASSERT(m_url);
        NX_VERBOSE(this, "Reconnecting directly to %1", *m_url);
        connectInternal([this](auto&&... /*args*/) {});
    }
}

void MediatorStunClient::connectWithResolving()
{
    NX_ASSERT(m_endpointProvider->getAioThread() == getAioThread());

    m_endpointProvider->fetchMediatorEndpoints(
        [this](auto... args) { onFetchEndpointCompletion(std::move(args)...); });
}

void MediatorStunClient::onFetchEndpointCompletion(
    nx::network::http::StatusCode::Value resultCode)
{
    if (!nx::network::http::StatusCode::isSuccessCode(resultCode))
        return failPendingRequests(SystemError::hostUnreachable);

    m_url = nx::network::url::Builder(m_endpointProvider->mediatorAddress()->tcpUrl)
        .appendPath(api::kStunOverHttpTunnelPath).toUrl();

    connectInternal([this](auto&&... /*args*/) {});

    sendPendingRequests();
}

void MediatorStunClient::failPendingRequests(
    SystemError::ErrorCode resultCode)
{
    auto postponedRequests = std::exchange(m_postponedRequests, {});
    for (auto& requestContext: postponedRequests)
        requestContext.handler(resultCode, nx::network::stun::Message());
}

void MediatorStunClient::sendPendingRequests()
{
    auto postponedRequests = std::exchange(m_postponedRequests, {});
    for (auto& requestContext: postponedRequests)
    {
        base_type::sendRequest(
            std::move(requestContext.request),
            std::move(requestContext.handler),
            requestContext.client);
    }
}

void MediatorStunClient::handleConnectCompletion(
    ConnectHandler handler,
    SystemError::ErrorCode resultCode)
{
    m_connected = resultCode == SystemError::noError;

    if (resultCode == SystemError::noError && m_keepAliveOptions)
        startKeepAliveProbing();

    if (resultCode != SystemError::noError)
        scheduleReconnect();

    if (handler)
        nx::utils::swapAndCall(handler, resultCode);
}

void MediatorStunClient::startKeepAliveProbing()
{
    NX_CRITICAL(m_connected && m_keepAliveOptions);

    m_alivenessTester = std::make_unique<nx::network::stun::ServerAlivenessTester>(
        *m_keepAliveOptions,
        &delegate());
    m_alivenessTester->bindToAioThread(getAioThread());
    m_alivenessTester->start([this]() { handleAlivenessTestFailure(); });
}

void MediatorStunClient::handleAlivenessTestFailure()
{
    NX_DEBUG(this, "Connection to %1 has failed keep-alive check",
        delegate().remoteAddress());

    stopKeepAliveProbing();

    delegate().closeConnection(SystemError::connectionReset);
}

void MediatorStunClient::stopKeepAliveProbing()
{
    m_alivenessTester.reset();
}

} // namespace nx::hpm::api
