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
    m_serverID(serverName.isEmpty() ? generateRandomName(16) : std::move(serverName))
{
    m_mediatorConnector.mockupAddress(std::move(mediatorEndpoint));

    m_mediatorConnector.setSystemCredentials(
        nx::hpm::MediatorConnector::SystemCredentials(
            m_systemData.id,
            m_serverID,
            m_systemData.authKey));
}

MediaServerEmulator::~MediaServerEmulator()
{
    if (m_systemClient)
        m_systemClient->pleaseStopSync();
}

bool MediaServerEmulator::start()
{
    if (!m_httpServer.bind(SocketAddress(HostAddress::localhost, 0)) ||
        !m_httpServer.listen())
    {
        return false;
    }

    m_systemClient = m_mediatorConnector.systemConnection();

    std::list<SocketAddress> localAddresses;
    localAddresses.push_back(m_httpServer.address());

    nx::hpm::api::ResultCode resultCode = nx::hpm::api::ResultCode::ok;
    bool result = false;
    std::tie(resultCode, result) = makeSyncCall<nx::hpm::api::ResultCode, bool>(
        std::bind(
            &nx::network::cloud::MediatorSystemConnection::bind,
            m_systemClient.get(),
            std::move(localAddresses),
            std::placeholders::_1));

    return resultCode == nx::hpm::api::ResultCode::ok;
}

nx::String MediaServerEmulator::serverID() const
{
    return m_serverID;
}

SocketAddress MediaServerEmulator::endpoint() const
{
    return m_httpServer.address();
}

}   //hpm
}   //nx
