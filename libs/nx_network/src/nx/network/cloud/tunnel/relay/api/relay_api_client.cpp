// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_client.h"

#include "detail/relay_api_client_factory.h"

namespace nx::cloud::relay::api {

Client::Client(
    const nx::utils::Url& baseUrl,
    std::optional<int> forcedHttpTunnelType)
    :
    m_actualClient(detail::ClientFactory::instance().create(baseUrl, forcedHttpTunnelType))
{
    bindToAioThread(getAioThread());
}

void Client::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_actualClient)
        m_actualClient->bindToAioThread(aioThread);
}

void Client::beginListening(
    const std::string& peerName,
    BeginListeningHandler handler)
{
    m_actualClient->beginListening(peerName, std::move(handler));
}

void Client::startSession(
    const std::string& desiredSessionId,
    const std::string& targetPeerName,
    StartClientConnectSessionHandler handler)
{
    m_actualClient->startSession(
        desiredSessionId,
        targetPeerName,
        std::move(handler));
}

void Client::openConnectionToTheTargetHost(
    const std::string& sessionId,
    OpenRelayConnectionHandler handler,
    const nx::network::http::tunneling::ConnectOptions& options)
{
    m_actualClient->openConnectionToTheTargetHost(
        sessionId,
        std::move(handler),
        options);
}

nx::utils::Url Client::url() const
{
    return m_actualClient->url();
}

SystemError::ErrorCode Client::prevRequestSysErrorCode() const
{
    return m_actualClient->prevRequestSysErrorCode();
}

nx::network::http::StatusCode::Value Client::prevRequestHttpStatusCode() const
{
    return m_actualClient->prevRequestHttpStatusCode();
}

void Client::setTimeout(std::optional<std::chrono::milliseconds> timeout)
{
    m_actualClient->setTimeout(timeout);
}

void Client::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_actualClient.reset();
}

} // namespace nx::cloud::relay::api
