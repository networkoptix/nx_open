#include "view.h"

#include <stdexcept>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/relaying/http_view/begin_listening_http_handler.h>

#include "http_handlers.h"
#include "../controller/connect_session_manager.h"
#include "../controller/controller.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {

View::View(
    const conf::Settings& settings,
    const Model& /*model*/,
    Controller* controller)
    :
    m_settings(settings),
    m_controller(controller),
    m_authenticationManager(m_authRestrictionList)
{
    registerApiHandlers();
    startAcceptor();
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

void View::registerApiHandlers()
{
    registerApiHandler<relaying::BeginListeningHandler>(
        nx_http::Method::post,
        &m_controller->listeningPeerManager());
    registerApiHandler<view::CreateClientSessionHandler>(
        nx_http::Method::post,
        &m_controller->connectSessionManager());
    registerApiHandler<view::ConnectToPeerHandler>(
        nx_http::Method::post,
        &m_controller->connectSessionManager());

    // TODO: #ak Following handlers are here for compatiblity with 3.1-beta. Remove after 3.1 release.
    registerApiHandler<relaying::BeginListeningHandler>(
        nx_http::Method::options,
        &m_controller->listeningPeerManager());
    registerApiHandler<view::ConnectToPeerHandler>(
        nx_http::Method::options,
        &m_controller->connectSessionManager());
}

template<typename Handler, typename Manager>
void View::registerApiHandler(
    const nx_http::StringType& method,
    Manager* manager)
{
    m_httpMessageDispatcher.registerRequestProcessor<Handler>(
        Handler::kPath,
        [this, manager]() -> std::unique_ptr<Handler>
        {
            return std::make_unique<Handler>(manager);
        },
        method);
}

void View::startAcceptor()
{
    const auto& httpEndpoints = m_settings.http().endpoints;
    if (httpEndpoints.empty())
    {
        NX_LOGX("No HTTP address to listen", cl_logALWAYS);
        throw std::runtime_error("No HTTP address to listen");
    }

    m_multiAddressHttpServer =
        std::make_unique<nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>>(
            &m_authenticationManager,
            &m_httpMessageDispatcher,
            false,
            nx::network::NatTraversalSupport::disabled);

    if (!m_multiAddressHttpServer->bind(httpEndpoints))
    {
        throw std::runtime_error(
            lm("Cannot bind to address(es) %1. %2")
                .container(httpEndpoints)
                .arg(SystemError::getLastOSErrorText()).toStdString());
    }
}

} // namespace relay
} // namespace cloud
} // namespace nx
