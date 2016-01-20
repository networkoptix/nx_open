/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#include "mediaserver_emulator.h"

#include <nx/network/cloud/data/result_code.h>
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
    m_httpServer(
        nullptr,
        &m_httpMessageDispatcher,
        false,
        SocketFactory::NatTraversalType::nttDisabled),
    m_systemData(std::move(systemData)),
    m_serverId(serverName.isEmpty() ? generateRandomName(16) : std::move(serverName)),
    m_mediatorUdpClient(mediatorEndpoint, &m_mediatorConnector)
{
    m_mediatorConnector.mockupAddress(std::move(mediatorEndpoint));

    m_mediatorConnector.setSystemCredentials(
        api::SystemCredentials(
            m_systemData.id,
            m_serverId,
            m_systemData.authKey));
}

MediaServerEmulator::~MediaServerEmulator()
{
    if (m_serverClient)
        m_serverClient->pleaseStopSync();
}

bool MediaServerEmulator::start()
{
    if (!m_httpServer.bind(SocketAddress(HostAddress::localhost, 0)) ||
        !m_httpServer.listen())
    {
        return false;
    }

    using namespace std::placeholders;
    m_serverClient = m_mediatorConnector.systemConnection();
    m_serverClient->setOnConnectionRequestedHandler(
        std::bind(&MediaServerEmulator::onConnectionRequested, this, _1));

    std::list<SocketAddress> localAddresses;
    localAddresses.push_back(m_httpServer.address());

    nx::hpm::api::ResultCode resultCode = nx::hpm::api::ResultCode::ok;
    std::tie(resultCode) = makeSyncCall<nx::hpm::api::ResultCode>(
        std::bind(
            &nx::hpm::api::MediatorServerTcpConnection::bind,
            m_serverClient.get(),
            std::move(localAddresses),
            std::placeholders::_1));

    return resultCode == nx::hpm::api::ResultCode::ok;
}

nx::String MediaServerEmulator::serverId() const
{
    return m_serverId;
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
    return m_mediatorUdpClient.localAddress();
}

void MediaServerEmulator::setOnConnectionRequestedHandler(
    std::function<void(nx::hpm::api::ConnectionRequestedEvent)> handler)
{
    m_onConnectionRequestedHandler = std::move(handler);
}

void MediaServerEmulator::onConnectionRequested(
    nx::hpm::api::ConnectionRequestedEvent connectionRequestedData)
{
    using namespace std::placeholders;

    nx::hpm::api::ConnectionAckRequest connectionAckData;
    connectionAckData.connectSessionID = connectionRequestedData.connectSessionID;
    connectionAckData.connectionMethods = nx::hpm::api::ConnectionMethod::udpHolePunching;
    m_mediatorUdpClient.connectionAck(
        std::move(connectionAckData),
        std::bind(&MediaServerEmulator::onConnectionAckResponseReceived, this, _1));

    if (m_onConnectionRequestedHandler)
        m_onConnectionRequestedHandler(std::move(connectionRequestedData));
}

void MediaServerEmulator::onConnectionAckResponseReceived(
    nx::hpm::api::ResultCode resultCode)
{
    //TODO
}

}   //hpm
}   //nx
