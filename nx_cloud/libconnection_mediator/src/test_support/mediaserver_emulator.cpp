/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#include "mediaserver_emulator.h"

#include <nx/network/cloud/data/result_code.h>
#include <nx/utils/thread/barrier_handler.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <utils/common/string.h>
#include <utils/common/sync_call.h>


namespace nx {
namespace hpm {

MediaServerEmulator::MediaServerEmulator(
    const SocketAddress& mediatorEndpoint,
    AbstractCloudDataProvider::System systemData,
    nx::String serverName)
:
    m_mediatorConnector(std::make_unique<hpm::api::MediatorConnector>()),
    m_httpServer(
        nullptr,
        &m_httpMessageDispatcher,
        false,
        SocketFactory::NatTraversalType::nttDisabled),
    m_systemData(std::move(systemData)),
    m_serverId(serverName.isEmpty() ? generateRandomName(16) : std::move(serverName)),
    m_mediatorUdpClient(
        std::make_unique<nx::hpm::api::MediatorServerUdpConnection>(
            mediatorEndpoint,
            m_mediatorConnector.get()))
{
    m_mediatorUdpClient->socket()->bindToAioThread(m_timer.getAioThread());

    m_mediatorConnector->mockupAddress(std::move(mediatorEndpoint));

    m_mediatorConnector->setSystemCredentials(
        api::SystemCredentials(
            m_systemData.id,
            m_serverId,
            m_systemData.authKey));
}

MediaServerEmulator::~MediaServerEmulator()
{
    pleaseStopSync();
}

bool MediaServerEmulator::start()
{
    if (!m_httpServer.bind(SocketAddress(HostAddress::localhost, 0)) ||
        !m_httpServer.listen())
    {
        return false;
    }

    using namespace std::placeholders;
    m_serverClient = m_mediatorConnector->systemConnection();
    m_serverClient->setOnConnectionRequestedHandler(
        std::bind(&MediaServerEmulator::onConnectionRequested, this, _1));
    return true;
}

nx::hpm::api::ResultCode MediaServerEmulator::registerOnMediator()
{
    std::list<SocketAddress> localAddresses;
    localAddresses.push_back(m_httpServer.address());

    nx::hpm::api::ResultCode resultCode = nx::hpm::api::ResultCode::ok;
    std::tie(resultCode) = makeSyncCall<nx::hpm::api::ResultCode>(
        std::bind(
            &nx::hpm::api::MediatorServerTcpConnection::bind,
            m_serverClient.get(),
            std::move(localAddresses),
            std::placeholders::_1));
    return resultCode;
}

nx::String MediaServerEmulator::serverId() const
{
    return m_serverId;
}

nx::String MediaServerEmulator::fullName() const
{
    return m_serverId + "." + m_systemData.id;
}

SocketAddress MediaServerEmulator::endpoint() const
{
    return m_httpServer.address();
}

nx::hpm::api::ResultCode MediaServerEmulator::listen() const
{
    api::ListenRequest requestData;
    requestData.systemId = m_systemData.id;
    requestData.serverId = m_serverId;
    nx::hpm::api::ResultCode resultCode = nx::hpm::api::ResultCode::ok;
    std::tie(resultCode) = makeSyncCall<nx::hpm::api::ResultCode>(
        std::bind(
            &nx::hpm::api::MediatorServerTcpConnection::listen,
            m_serverClient.get(),
            std::move(requestData),
            std::placeholders::_1));

    return resultCode;
}

SocketAddress MediaServerEmulator::mediatorConnectionLocalAddress() const
{
    return m_serverClient->localAddress();
}

SocketAddress MediaServerEmulator::udpHolePunchingEndpoint() const
{
    return m_mediatorUdpClient->localAddress();
}

void MediaServerEmulator::setOnConnectionRequestedHandler(
    std::function<ActionToTake(nx::hpm::api::ConnectionRequestedEvent)> handler)
{
    m_onConnectionRequestedHandler = std::move(handler);
}

void MediaServerEmulator::setConnectionAckResponseHandler(
    std::function<ActionToTake(api::ResultCode)> handler)
{
    m_connectionAckResponseHandler = std::move(handler);
}

void MediaServerEmulator::onConnectionRequested(
    nx::hpm::api::ConnectionRequestedEvent connectionRequestedData)
{
    using namespace std::placeholders;

    m_connectionRequestedData = connectionRequestedData;

    nx::hpm::api::ConnectionAckRequest connectionAckData;
    connectionAckData.connectSessionId = connectionRequestedData.connectSessionId;
    connectionAckData.connectionMethods = nx::hpm::api::ConnectionMethod::udpHolePunching;

    if (m_onConnectionRequestedHandler)
    {
        const auto actionToTake = m_onConnectionRequestedHandler(
            std::move(connectionRequestedData));
        switch (actionToTake)
        {
            case ActionToTake::proceedWithConnection:
                break;
            case ActionToTake::ignoreIndication:
                return;
            case ActionToTake::closeConnectionToMediator:
                m_serverClient.reset();
                m_mediatorConnector.reset();
                return;
            default:
                NX_ASSERT(false);
        }
    }

    m_mediatorUdpClient->connectionAck(
        std::move(connectionAckData),
        std::bind(&MediaServerEmulator::onConnectionAckResponseReceived, this, _1));
}

void MediaServerEmulator::onConnectionAckResponseReceived(
    nx::hpm::api::ResultCode resultCode)
{
    ActionToTake action = ActionToTake::ignoreIndication;

    if (m_connectionAckResponseHandler)
        action = m_connectionAckResponseHandler(resultCode);

    if (action != ActionToTake::proceedWithConnection)
        return;

    if (m_connectionRequestedData.udpEndpointList.empty())
        return;

    //connecting to the originating peer
    auto udtStreamSocket = std::make_unique<nx::network::UdtStreamSocket>();
    auto mediatorUdpClientSocket = m_mediatorUdpClient->takeSocket();
    m_mediatorUdpClient.reset();
    if (!udtStreamSocket->bindToUdpSocket(std::move(*mediatorUdpClientSocket)) ||
        !udtStreamSocket->setNonBlockingMode(true) ||
        !udtStreamSocket->setRendezvous(true))
    {
        return;
    }

    auto udtStreamServerSocket = std::make_unique<nx::network::UdtStreamServerSocket>();
    if (!udtStreamServerSocket->setReuseAddrFlag(true) ||
        !udtStreamServerSocket->bind(udtStreamSocket->getLocalAddress()) ||
        !udtStreamServerSocket->setNonBlockingMode(true))
    {
        return;
    }

    m_udtStreamSocket = std::move(udtStreamSocket);
    m_udtStreamServerSocket = std::move(udtStreamServerSocket);

    using namespace std::placeholders;
    m_udtStreamSocket->connectAsync(
        m_connectionRequestedData.udpEndpointList.front(),
        std::bind(&MediaServerEmulator::onUdtConnectDone, this, _1));
    m_udtStreamServerSocket->acceptAsync(
        std::bind(&MediaServerEmulator::onUdtConnectionAccepted, this, _1, _2));
}

void MediaServerEmulator::onUdtConnectDone(SystemError::ErrorCode /*errorCode*/)
{
    //TODO #ak
}

void MediaServerEmulator::onUdtConnectionAccepted(
    SystemError::ErrorCode /*errorCode*/,
    AbstractStreamSocket* acceptedSocket)
{
    delete acceptedSocket;
}

void MediaServerEmulator::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_timer.pleaseStop(
        [this, completionHandler = move(completionHandler)]() mutable
        {
            if (m_mediatorUdpClient)
                m_mediatorUdpClient.reset();

            if (!m_serverClient && !m_udtStreamSocket)
                return completionHandler();

            nx::BarrierHandler barrier(std::move(completionHandler));
            if (m_serverClient)
                m_serverClient->pleaseStop(barrier.fork());
            if (m_udtStreamSocket)
                m_udtStreamSocket->pleaseStop(barrier.fork());
            if (m_udtStreamServerSocket)
                m_udtStreamServerSocket->pleaseStop(barrier.fork());
        });
}

}   //hpm
}   //nx
