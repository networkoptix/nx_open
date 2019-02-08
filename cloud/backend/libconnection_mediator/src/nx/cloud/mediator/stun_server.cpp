#include "stun_server.h"

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

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
        nx::network::NatTraversalSupport::disabled))
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
                .arg(containerString(m_tcpEndpoints)).toStdString());
    }

    if (m_udpStunServer && !m_udpStunServer->listen())
    {
        throw std::runtime_error(
            lm("Error listening on UDP addresses %1")
                .arg(containerString(m_udpEndpoints)).toStdString());
    }

    NX_ALWAYS(this, lm("STUN Server is listening on tcp/%1 and udp/%2")
        .args(containerString(m_tcpEndpoints), containerString(m_udpEndpoints)));
}

void StunServer::stopAcceptingNewRequests()
{
    m_tcpStunServer->pleaseStopSync();

    if (m_udpStunServer)
        m_udpStunServer->forEachListener(&network::stun::UdpServer::stopReceivingMessagesSync);
}

const std::vector<network::SocketAddress>& StunServer::udpEndpoints() const
{
    return m_udpEndpoints;
}

const std::vector<network::SocketAddress>& StunServer::tcpEndpoints() const
{
    return m_tcpEndpoints;
}

network::stun::MessageDispatcher& StunServer::dispatcher()
{
    return m_stunMessageDispatcher;
}

const StunServer::MultiAddressStunServer& StunServer::server() const
{
    return *m_tcpStunServer;
}

void StunServer::initializeHttpTunnelling(http::Server* httpServer)
{
    m_stunOverHttpServer.setupHttpTunneling(
        &httpServer->messageDispatcher(),
        network::url::joinPath(api::kMediatorApiPrefix, api::kStunOverHttpTunnelPath).c_str());
}

bool StunServer::bind()
{
    if (m_settings.stun().addrToListenList.empty())
    {
        NX_ALWAYS(this, "No STUN address to listen");
        return false;
    }

    if (!m_tcpStunServer->bind(m_settings.stun().addrToListenList))
    {
        NX_ERROR(this, lm("Cannot bind to TCP endpoint(s): %1")
            .args(containerString(m_settings.stun().addrToListenList)));
        return false;
    }

    m_tcpStunServer->forEachListener(
        [this](network::stun::SocketServer* server)
        {
            server->setConnectionInactivityTimeout(
                m_settings.stun().connectionInactivityTimeout);
        });

    m_tcpEndpoints = m_tcpStunServer->endpoints();

    if (!m_settings.stun().udpAddrToListenList.empty())
    {
        m_udpStunServer =
            std::make_unique<nx::network::server::MultiAddressServer<network::stun::UdpServer>>(
                &m_stunMessageDispatcher);

        if (!m_udpStunServer->bind(m_settings.stun().udpAddrToListenList))
        {
            NX_ERROR(this, lm("Cannot bind to UDP endpoint(s): %1")
                .args(containerString(m_settings.stun().udpAddrToListenList)));
            return false;
        }

        m_udpEndpoints = m_udpStunServer->endpoints();
    }

    return true;
}

} // namespace hpm
} // namespace nx
