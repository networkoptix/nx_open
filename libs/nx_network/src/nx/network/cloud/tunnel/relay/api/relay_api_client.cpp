#include "relay_api_client.h"

#include "detail/relay_api_client_factory.h"

namespace nx::cloud::relay::api {

Client::Client(const nx::utils::Url& baseUrl):
    m_actualClient(detail::ClientFactory::instance().create(baseUrl))
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
    OpenRelayConnectionHandler handler)
{
    m_actualClient->openConnectionToTheTargetHost(
        sessionId,
        std::move(handler));
}

nx::utils::Url Client::url() const
{
    return m_actualClient->url();
}

SystemError::ErrorCode Client::prevRequestSysErrorCode() const
{
    return m_actualClient->prevRequestSysErrorCode();
}

void Client::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_actualClient.reset();
}

} // namespace nx::cloud::relay::api
