#include "stun_server.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "http/http_api_path.h"
#include "http/http_server.h"
#include "settings.h"

namespace nx {
namespace hpm {

StunServer::StunServer(
    const conf::Settings& settings,
    http::Server* httpServer)
    :
    m_settings(settings),
    m_stunOverHttpServer(&m_stunMessageDispatcher),
    m_tcpStunServer(std::make_unique<nx::network::server::MultiAddressServer<network::stun::SocketServer>>(
        &m_stunMessageDispatcher,
        /* ssl required? */ false,
        nx::network::NatTraversalSupport::disabled)),
    m_udpStunServer(std::make_unique<nx::network::server::MultiAddressServer<network::stun::UdpServer>>(
        &m_stunMessageDispatcher))
{
    if (!bind())
        throw std::runtime_error("Error binding to specified STUN address");

    initializeHttpTunnelling(httpServer);
}

void StunServer::listen()
{
    if (!m_tcpStunServer->listen())
    {
        throw std::runtime_error(
            lm("Error listening on TCP addresses %1")
                .arg(containerString(m_endpoints)).toStdString());
    }

    if (!m_udpStunServer->listen())
    {
        throw std::runtime_error(
            lm("Error listening on UDP addresses %1")
                .arg(containerString(m_endpoints)).toStdString());
    }

    NX_LOGX(lit("STUN Server is listening on %1")
        .arg(containerString(m_endpoints)), cl_logALWAYS);
}

void StunServer::stopAcceptingNewRequests()
{
    m_tcpStunServer->pleaseStopSync();
    m_udpStunServer->forEachListener(&network::stun::UdpServer::stopReceivingMessagesSync);
}

const std::vector<network::SocketAddress>& StunServer::endpoints() const
{
    return m_endpoints;
}

network::stun::MessageDispatcher& StunServer::dispatcher()
{
    return m_stunMessageDispatcher;
}

void StunServer::initializeHttpTunnelling(http::Server* httpServer)
{
    m_stunOverHttpServer.setupHttpTunneling(
        &httpServer->messageDispatcher(),
        http::kStunOverHttpTunnelPath);
}

bool StunServer::bind()
{
    if (m_settings.stun().addrToListenList.empty())
    {
        NX_LOGX("No STUN address to listen", cl_logALWAYS);
        return false;
    }

    m_tcpStunServer->forEachListener(
        [this](network::stun::SocketServer* server)
        {
            server->setConnectionInactivityTimeout(m_settings.stun().connectionInactivityTimeout);
        });

    if (!m_tcpStunServer->bind(m_settings.stun().addrToListenList))
    {
        NX_LOGX(lit("Can not bind to TCP addresses: %1")
            .arg(containerString(m_settings.stun().addrToListenList)), cl_logERROR);
        return false;
    }

    m_endpoints = m_tcpStunServer->endpoints();

    if (!m_udpStunServer->bind(m_endpoints /*m_settings.stun().addrToListenList*/))
    {
        NX_LOGX(lit("Can not bind to UDP addresses: %1")
            .arg(containerString(m_settings.stun().addrToListenList)), cl_logERROR);
        return false;
    }

    return true;
}

} // namespace hpm
} // namespace nx
