/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#include "mediaserver_emulator.h"

#include <nx/network/cloud_connectivity/data/result_code.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <utils/common/sync_call.h>


namespace nx {
namespace hpm {

MediaServerEmulator::MediaServerEmulator(
    const SocketAddress& mediatorEndpoint,
    AbstractCloudDataProvider::System systemData)
:
    m_httpServer(
        nullptr,
        &m_httpMessageDispatcher,
        false,
        SocketFactory::NatTraversalType::nttDisabled),
    m_systemData(std::move(systemData)),
    m_serverID(generateSalt(16))
{
    m_mediatorConnector.mockupAddress(std::move(mediatorEndpoint));

    m_mediatorConnector.setSystemCredentials(
        nx::cc::MediatorConnector::SystemCredentials(
            m_systemData.id,
            m_serverID,
            m_systemData.authKey));

    //TODO #ak registering HTTP handler in m_httpMessageDispatcher
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

    nx::cc::api::ResultCode resultCode = nx::cc::api::ResultCode::ok;
    bool result = false;
    std::tie(resultCode, result) = makeSyncCall<nx::cc::api::ResultCode, bool>(
        std::bind(
            &nx::cc::MediatorSystemConnection::bind,
            m_systemClient.get(),
            std::move(localAddresses),
            std::placeholders::_1));

    return resultCode == nx::cc::api::ResultCode::ok;
}

}   //hpm
}   //nx
