#include "connection_mediaton_initiator.h"

#include "../cloud_connect_settings.h"

namespace nx::network::cloud {

ConnectionMediationInitiator::ConnectionMediationInitiator(
    const CloudConnectSettings& settings,
    const hpm::api::MediatorAddress& mediatorAddress,
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> mediatorUdpClient)
    :
    m_settings(settings),
    m_mediatorAddress(mediatorAddress),
    m_mediatorUdpClient(std::move(mediatorUdpClient))
{
    if (m_mediatorUdpClient)
        m_mediatorUdpClient->bindToAioThread(getAioThread());
}

void ConnectionMediationInitiator::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_mediatorUdpClient)
        m_mediatorUdpClient->bindToAioThread(aioThread);

    if (m_udpHolePunchingSocket)
        m_udpHolePunchingSocket->bindToAioThread(aioThread);

    m_tcpConnectDelayTimer.bindToAioThread(aioThread);

    if (m_mediatorApiClient)
        m_mediatorApiClient->bindToAioThread(aioThread);
}

void ConnectionMediationInitiator::start(
    const hpm::api::ConnectRequest& request,
    Handler handler)
{
    m_request = request;
    m_handler = std::move(handler);

    initiateConnectOverUdp();

    // TODO: #ak If UDP request has failed before timed, invoke initiateConnectOverTcp ASAP.

    m_tcpConnectDelayTimer.start(
        m_settings.delayBeforeSendingConnectToMediatorOverTcp,
        [this]() mutable { initiateConnectOverTcp(); });
}

std::unique_ptr<UDPSocket> ConnectionMediationInitiator::takeUdpSocket()
{
    return std::exchange(m_udpHolePunchingSocket, nullptr);
}

hpm::api::MediatorAddress ConnectionMediationInitiator::mediatorLocation() const
{
    return m_mediatorAddress;
}

void ConnectionMediationInitiator::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_mediatorUdpClient.reset();
    m_udpHolePunchingSocket.reset();
    m_tcpConnectDelayTimer.pleaseStopSync();
    m_mediatorApiClient.reset();
}

void ConnectionMediationInitiator::initiateConnectOverUdp()
{
    if (!m_mediatorUdpClient)
        return;

    m_mediatorUdpClient->connect(
        m_request,
        [this](auto&&... args) { handleResponseOverUdp(std::move(args)...); });
}

void ConnectionMediationInitiator::initiateConnectOverTcp()
{
    m_mediatorApiClient = std::make_unique<nx::hpm::api::Client>(
        m_mediatorAddress.tcpUrl);
    m_mediatorApiClient->bindToAioThread(getAioThread());

    m_mediatorApiClient->initiateConnection(
        m_request,
        [this](auto&&... args)
        {
            m_tcpRequestCompleted = true;
            handleResponse(std::move(args)...);
        });
}

void ConnectionMediationInitiator::handleResponseOverUdp(
    stun::TransportHeader stunTransportHeader,
    hpm::api::ResultCode resultCode,
    hpm::api::ConnectResponse response)
{
    if (resultCode == hpm::api::ResultCode::networkError &&
        !m_tcpRequestCompleted)
    {
        if (!m_mediatorApiClient) //< TCP request has not been started yet.
        {
            // Cancelling delay and sending request right away.
            m_tcpConnectDelayTimer.pleaseStopSync();
            initiateConnectOverTcp();
        }
        return;
    }

    m_mediatorAddress.stunUdpEndpoint = stunTransportHeader.locationEndpoint;
    m_udpHolePunchingSocket = m_mediatorUdpClient->takeSocket();

    handleResponse(resultCode, std::move(response));
}

void ConnectionMediationInitiator::handleResponse(
    nx::hpm::api::ResultCode resultCode,
    nx::hpm::api::ConnectResponse response)
{
    m_mediatorUdpClient.reset();
    m_tcpConnectDelayTimer.pleaseStopSync();
    m_mediatorApiClient.reset();

    nx::utils::swapAndCall(m_handler, resultCode, std::move(response));
}

} // namespace nx::network::cloud
