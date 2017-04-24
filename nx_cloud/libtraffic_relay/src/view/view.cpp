#include "view.h"

#include <stdexcept>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {

View::View(
    const conf::Settings& settings,
    const Model& /*model*/,
    Controller* /*controller*/)
    :
    m_settings(settings),
    m_authenticationManager(m_authRestrictionList)
{
    const auto& httpEndpoints = settings.http().endpoints;
    if (httpEndpoints.empty())
    {
        NX_LOGX("No HTTP address to listen", cl_logALWAYS);
        throw std::runtime_error("No HTTP address to listen");
    }

    //registerApiHandlers(&m_httpMessageDispatcher);

    //m_authRestrictionList.allow(http_handler::GetCloudModulesXml::kHandlerPath, AuthMethod::noAuth);

    m_multiAddressHttpServer =
        std::make_unique<MultiAddressServer<nx_http::HttpStreamSocketServer>>(
            &m_authenticationManager,
            &m_httpMessageDispatcher,
            false,
            nx::network::NatTraversalSupport::disabled);

    if (!m_multiAddressHttpServer->bind(httpEndpoints))
    {
        throw std::runtime_error(
            lm("Cannot bind to address(es) %1. %2")
            .container(httpEndpoints).arg(SystemError::getLastOSErrorText()).toStdString());
    }
}

View::~View()
{
    m_multiAddressHttpServer->forEachListener(
        [](nx_http::HttpStreamSocketServer* listener)
        {
            listener->pleaseStopSync();
        });
}

void View::start()
{
    if (!m_multiAddressHttpServer->listen(m_settings.http().tcpBacklogSize))
    {
        throw std::runtime_error(
            lm("Cannot start listening: %1")
            .arg(SystemError::getLastOSErrorText()).toStdString());
    }
}

std::vector<SocketAddress> View::httpEndpoints() const
{
    return m_multiAddressHttpServer->endpoints();
}

} // namespace relay
} // namespace cloud
} // namespace nx
