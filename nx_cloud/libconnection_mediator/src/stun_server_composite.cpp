#include "stun_server_composite.h"

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>

#include "settings.h"

namespace nx {
namespace hpm {

StunServer::StunServer(const conf::Settings& settings):
    m_settings(settings),
    m_tcpStunServer(std::make_unique<MultiAddressServer<stun::SocketServer>>(
        &m_stunMessageDispatcher,
        false,
        nx::network::NatTraversalSupport::disabled)),
    m_udpStunServer(std::make_unique<MultiAddressServer<stun::UdpServer>>(
        &m_stunMessageDispatcher))
{
}

bool StunServer::bind()
{
    //accepting STUN requests by both tcp and udt

    if (!m_tcpStunServer->bind(m_settings.stun().addrToListenList))
    {
        NX_LOGX(lit("Can not bind to TCP addresses: %1")
            .arg(containerString(m_settings.stun().addrToListenList)), cl_logERROR);
        return false;
    }

    m_endpoints = m_tcpStunServer->endpoints();
    m_tcpStunServer->forEachListener(
        [this](stun::SocketServer* server)
        {
            server->setConnectionKeepAliveOptions(m_settings.stun().keepAliveOptions);
        });

    if (!m_udpStunServer->bind(m_endpoints /*m_settings.stun().addrToListenList*/))
    {
        NX_LOGX(lit("Can not bind to UDP addresses: %1")
            .arg(containerString(m_settings.stun().addrToListenList)), cl_logERROR);
        return false;
    }

    return true;
}

bool StunServer::listen()
{
    if (!m_tcpStunServer->listen())
    {
        NX_LOGX(lit("Can not listen on TCP addresses %1")
            .arg(containerString(m_endpoints)), cl_logERROR);
        return false;
    }

    if (!m_udpStunServer->listen())
    {
        NX_LOGX(lit("Can not listen on UDP addresses %1")
            .arg(containerString(m_endpoints)), cl_logERROR);
        return false;
    }

    return true;
}

void StunServer::stopAcceptingNewRequests()
{
    m_tcpStunServer->pleaseStopSync();
    m_udpStunServer->forEachListener(&stun::UdpServer::stopReceivingMessagesSync);
}

const std::vector<SocketAddress>& StunServer::endpoints() const
{
    return m_endpoints;
}

nx::stun::MessageDispatcher& StunServer::dispatcher()
{
    return m_stunMessageDispatcher;
}

} // namespace hpm
} // namespace nx
