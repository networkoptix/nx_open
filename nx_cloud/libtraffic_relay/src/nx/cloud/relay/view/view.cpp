#include "view.h"

#include <stdexcept>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/relaying/http_view/begin_listening_http_handler.h>

#include "http_handlers.h"
#include "proxy_handler.h"
#include "../controller/connect_session_manager.h"
#include "../controller/controller.h"
#include "../model/model.h"
#include "../settings.h"
#include "../statistics_provider.h"

namespace nx {
namespace cloud {
namespace relay {

template<typename ResultType>
class GetHandler:
    public nx::network::http::AbstractFusionRequestHandler<void, ResultType>
{
public:
    using FunctorType = nx::utils::MoveOnlyFunc<ResultType()>;

    GetHandler(FunctorType func):
        m_func(std::move(func))
    {
    }

private:
    FunctorType m_func;

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        const nx::network::http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/) override
    {
        auto data = m_func();
        this->requestCompleted(nx::network::http::FusionRequestResult(), std::move(data));
    }
};

//-------------------------------------------------------------------------------------------------

View::View(
    const conf::Settings& settings,
    Model* model,
    Controller* controller)
    :
    m_settings(settings),
    m_model(model),
    m_controller(controller),
    m_authenticationManager(m_authRestrictionList)
{
    registerApiHandlers();
    startAcceptor();
}

View::~View()
{
    m_multiAddressHttpServer->forEachListener(
        [](nx::network::http::HttpStreamSocketServer* listener)
        {
            listener->pleaseStopSync();
        });
}

void View::registerStatisticsApiHandlers(
    const AbstractStatisticsProvider& statisticsProvider)
{
    using GetAllStatisticsHandler = GetHandler<Statistics>;

    registerApiHandler<GetAllStatisticsHandler>(
        api::kRelayStatisticsMetricsPath,
        nx::network::http::Method::get,
        std::bind(&AbstractStatisticsProvider::getAllStatistics, &statisticsProvider));
}

void View::start()
{
    m_multiAddressHttpServer->forEachListener(
        &nx::network::http::HttpStreamSocketServer::setConnectionInactivityTimeout,
        m_settings.http().connectionInactivityTimeout);

    if (!m_multiAddressHttpServer->listen(m_settings.http().tcpBacklogSize))
    {
        throw std::system_error(
            SystemError::getLastOSErrorCode(),
            std::system_category(),
            lm("Cannot start listening: %1")
                .args(SystemError::getLastOSErrorText()).toStdString());
    }
}

std::vector<network::SocketAddress> View::httpEndpoints() const
{
    return m_multiAddressHttpServer->endpoints();
}

const View::MultiHttpServer& View::httpServer() const
{
    return *m_multiAddressHttpServer;
}

void View::registerApiHandlers()
{
    registerApiHandler<relaying::BeginListeningHandler>(
        nx::network::http::Method::post,
        &m_controller->listeningPeerManager());
    registerApiHandler<view::CreateClientSessionHandler>(
        nx::network::http::Method::post,
        &m_controller->connectSessionManager());
    registerApiHandler<view::ConnectToPeerHandler>(
        nx::network::http::Method::post,
        &m_controller->connectSessionManager());

    registerApiHandler<relaying::BeginListeningUsingConnectMethodHandler>(
        nx::network::http::Method::connect,
        &m_controller->listeningPeerManager());

    // TODO: #ak Following handlers are here for compatibility with 3.1-beta.
    // Keep until 3.2 release just in case.
    registerCompatibilityHandlers();

    m_httpMessageDispatcher.registerRequestProcessor<view::ProxyHandler>(
        network::http::kAnyPath,
        [this]() -> std::unique_ptr<view::ProxyHandler>
        {
            return std::make_unique<view::ProxyHandler>(
                &m_model->listeningPeerPool());
        });
}

void View::registerCompatibilityHandlers()
{
    registerApiHandler<relaying::BeginListeningHandler>(
        nx::network::http::Method::options,
        &m_controller->listeningPeerManager());
    registerApiHandler<view::ConnectToPeerHandler>(
        nx::network::http::Method::options,
        &m_controller->connectSessionManager());
}

template<typename Handler, typename Arg>
void View::registerApiHandler(
    const nx::network::http::StringType& method,
    Arg arg)
{
    registerApiHandler<Handler, Arg>(Handler::kPath, method, std::move(arg));
}

template<typename Handler, typename Arg>
void View::registerApiHandler(
    const char* path,
    const nx::network::http::StringType& method,
    Arg arg)
{
    m_httpMessageDispatcher.registerRequestProcessor<Handler>(
        path,
        [this, arg]() -> std::unique_ptr<Handler>
        {
            return std::make_unique<Handler>(arg);
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
        std::make_unique<nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>>(
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
