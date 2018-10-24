#include "connection_mediaton_initiator.h"

namespace nx::network::cloud {

ConnectionMediationInitiator::ConnectionMediationInitiator(
    const hpm::api::MediatorAddress& mediatorAddress,
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> mediatorUdpClient)
    :
    m_mediatorAddress(mediatorAddress),
    m_mediatorUdpClient(std::move(mediatorUdpClient))
{
}

void ConnectionMediationInitiator::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_mediatorUdpClient)
        m_mediatorUdpClient->bindToAioThread(aioThread);
}

void ConnectionMediationInitiator::go(
    const hpm::api::ConnectRequest& request, 
    Handler handler)
{
    m_handler = std::move(handler);

    initiateConnectOverUdp(request);
}

std::unique_ptr<UDPSocket> ConnectionMediationInitiator::takeUdpSocket()
{
    if (!m_mediatorUdpClient)
        return nullptr;

    auto udpSocket = m_mediatorUdpClient->takeSocket();
    m_mediatorUdpClient.reset();
    return udpSocket;
}

hpm::api::MediatorAddress ConnectionMediationInitiator::mediatorLocation() const
{
    return m_mediatorAddress;
}

void ConnectionMediationInitiator::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_mediatorUdpClient.reset();
}

void ConnectionMediationInitiator::initiateConnectOverUdp(
    hpm::api::ConnectRequest request)
{
    m_mediatorUdpClient->connect(
        std::move(request),
        [this](auto&&... args) { handleResponseOverUdp(std::move(args)...); });
}

void ConnectionMediationInitiator::handleResponseOverUdp(
    stun::TransportHeader stunTransportHeader,
    nx::hpm::api::ResultCode resultCode,
    nx::hpm::api::ConnectResponse response)
{
    m_mediatorAddress.stunUdpEndpoint = stunTransportHeader.locationEndpoint;

    nx::utils::swapAndCall(m_handler, resultCode, std::move(response));
}

} // namespace nx::network::cloud
