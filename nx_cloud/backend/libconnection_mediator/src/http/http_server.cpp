#include "http_server.h"

#include <memory>

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "get_listening_peer_list_handler.h"
#include "../controller.h"
#include "../statistics/statistics_provider.h"

namespace nx {
namespace hpm {
namespace http {

// TODO: #ak Consider moving this class to some common location.
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

Server::Server(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool)
    :
    m_settings(settings)
{
    NX_ASSERT(!m_settings.http().addrToListenList.empty());

    if (!launchHttpServerIfNeeded(m_settings, peerRegistrator, registeredPeerPool))
        throw std::runtime_error("Failed to initialize http server");
}

void Server::listen()
{
    if (!m_multiAddressHttpServer->listen())
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        throw std::runtime_error(
            lm("Error listening on HTTP addresses %1. %2")
                .arg(containerString(m_settings.http().addrToListenList))
                .arg(SystemError::toString(osErrorCode)).toStdString());
    }

    NX_ALWAYS(this, lm("HTTP server is listening on %1")
        .args(containerString(m_multiAddressHttpServer->endpoints())));
}

void Server::stopAcceptingNewRequests()
{
    m_multiAddressHttpServer->pleaseStopSync();
}

nx::network::http::server::rest::MessageDispatcher& Server::messageDispatcher()
{
    return *m_httpMessageDispatcher;
}

std::vector<network::SocketAddress> Server::endpoints() const
{
    return m_endpoints;
}

const Server::MultiAddressHttpServer& Server::server() const
{
    return *m_multiAddressHttpServer;
}

void Server::registerStatisticsApiHandlers(const stats::Provider& provider)
{
    using GetAllStatisticsHandler = GetHandler<stats::Statistics>;

    registerApiHandler<GetAllStatisticsHandler>(
        network::url::joinPath(api::kMediatorApiPrefix, api::kStatisticsMetricsPath).c_str(),
        nx::network::http::Method::get,
        std::bind(&stats::Provider::getAllStatistics, &provider));
}

bool Server::launchHttpServerIfNeeded(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool)
{
    NX_INFO(this, "Bringing up HTTP server");

    m_httpMessageDispatcher = std::make_unique<nx::network::http::server::rest::MessageDispatcher>();

    // Registering HTTP handlers.
    m_httpMessageDispatcher->registerRequestProcessor<http::GetListeningPeerListHandler>(
        network::url::joinPath(api::kMediatorApiPrefix, GetListeningPeerListHandler::kHandlerPath).c_str(),
        [&]() { return std::make_unique<http::GetListeningPeerListHandler>(peerRegistrator); });

    m_multiAddressHttpServer =
        std::make_unique<nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>>(
            nullptr, //< TODO: #ak Add authentication.
            m_httpMessageDispatcher.get(),
            /*ssl required*/ false,
            nx::network::NatTraversalSupport::disabled);

    if (!m_multiAddressHttpServer->bind(settings.http().addrToListenList))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_ERROR(this, lm("Failed to bind HTTP server to address ... . %1")
            .arg(SystemError::toString(osErrorCode)));
        return false;
    }

    m_endpoints = m_multiAddressHttpServer->endpoints();
    m_multiAddressHttpServer->forEachListener(
        [&settings](nx::network::http::HttpStreamSocketServer* server)
        {
            server->setConnectionKeepAliveOptions(settings.http().keepAliveOptions);
        });

    m_discoveryHttpServer = std::make_unique<nx::cloud::discovery::HttpServer>(
        m_httpMessageDispatcher.get(),
        registeredPeerPool);

    return true;
}

template<typename Handler, typename Arg>
void Server::registerApiHandler(
    const char* path,
    const nx::network::http::StringType& method,
    Arg arg)
{
    m_httpMessageDispatcher->registerRequestProcessor<Handler>(
        path,
        [this, arg]() -> std::unique_ptr<Handler>
        {
            return std::make_unique<Handler>(arg);
        },
        method);
}

} // namespace http
} // namespace hpm
} // namespace nx
