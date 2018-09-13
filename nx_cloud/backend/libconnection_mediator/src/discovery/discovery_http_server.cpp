#include "discovery_http_server.h"

#include <boost/algorithm/string.hpp>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/std/cpp14.h>

#include "discovery_http_api_path.h"
#include "registered_peer_pool.h"

namespace nx {
namespace cloud {
namespace discovery {

namespace {

class GetDiscoveredModulesHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    GetDiscoveredModulesHandler(RegisteredPeerPool* registeredPeerPool):
        m_registeredPeerPool(registeredPeerPool)
    {
    }

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        // TODO Filter by peer type.

        const auto peerInfoList = m_registeredPeerPool->getPeerInfoList();
        std::string peerInfoListJson;
        peerInfoListJson += "[";
        peerInfoListJson += boost::join(peerInfoList, ",");
        peerInfoListJson += "]";

        nx::network::http::RequestResult requestResult(nx::network::http::StatusCode::ok);
        requestResult.dataSource = std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            nx::Buffer::fromStdString(peerInfoListJson));
        completionHandler(std::move(requestResult));
    }

private:
    RegisteredPeerPool* m_registeredPeerPool;
};

class GetDiscoveredModuleHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    GetDiscoveredModuleHandler(RegisteredPeerPool* registeredPeerPool):
        m_registeredPeerPool(registeredPeerPool)
    {
    }

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        if (requestPathParams().empty())
            return completionHandler(nx::network::http::StatusCode::badRequest);

        auto peerInfo = m_registeredPeerPool->getPeerInfo(requestPathParams()[0].toStdString());
        if (!peerInfo)
            return completionHandler(nx::network::http::StatusCode::notFound);

        nx::network::http::RequestResult requestResult(nx::network::http::StatusCode::ok);
        requestResult.dataSource = std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            nx::Buffer::fromStdString(*peerInfo));
        completionHandler(std::move(requestResult));
    }

private:
    RegisteredPeerPool* m_registeredPeerPool;
};

} // namespace

//-------------------------------------------------------------------------------------------------

HttpServer::HttpServer(
    nx::network::http::server::rest::MessageDispatcher* httpMessageDispatcher,
    RegisteredPeerPool* registeredPeerPool)
    :
    m_httpMessageDispatcher(httpMessageDispatcher),
    m_registeredPeerPool(registeredPeerPool)
{
    using namespace std::placeholders;

    m_httpMessageDispatcher->registerRequestProcessor<GetDiscoveredModulesHandler>(
        http::kDiscoveredModulesPath,
        [this](){ return std::make_unique<GetDiscoveredModulesHandler>(m_registeredPeerPool); },
        nx::network::http::Method::get);

    m_httpMessageDispatcher->registerRequestProcessor<nx::network::websocket::server::AcceptConnectionHandler>(
        http::kModuleKeepAliveConnectionPath,
        [this]()
        {
            return std::make_unique<nx::network::websocket::server::AcceptConnectionHandler>(
                std::bind(&HttpServer::onKeepAliveConnectionAccepted, this, _1, _2));
        },
        nx::network::http::Method::post);

    m_httpMessageDispatcher->registerRequestProcessor<GetDiscoveredModuleHandler>(
        http::kDiscoveredModulePath,
        [this]() { return std::make_unique<GetDiscoveredModuleHandler>(m_registeredPeerPool); },
        nx::network::http::Method::get);
}

void HttpServer::onKeepAliveConnectionAccepted(
    std::unique_ptr<nx::network::WebSocket> connection,
    std::vector<nx::network::http::StringType> restParams)
{
    if (restParams.empty())
        return;

    m_registeredPeerPool->addPeerConnection(
        restParams[0].toStdString(),
        std::move(connection));
}

} // namespace discovery
} // namespace cloud
} // namespace nx
