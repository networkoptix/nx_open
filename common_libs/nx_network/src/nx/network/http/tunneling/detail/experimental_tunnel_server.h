#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "basic_custom_tunnel_server.h"
#include "request_paths.h"
#include "separate_up_down_channel_delegate.h"
#include "../abstract_tunnel_authorizer.h"
#include "../../http_types.h"
#include "../../server/http_server_connection.h"
#include "../../server/http_stream_socket_server.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling::detail {

template<typename ...ApplicationData>
class ExperimentalTunnelServer:
    public BasicCustomTunnelServer<ApplicationData...>
{
    using base_type = BasicCustomTunnelServer<ApplicationData...>;

public:
    ExperimentalTunnelServer(typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~ExperimentalTunnelServer();

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

private:
    struct TunnelContext
    {
        std::unique_ptr<AbstractStreamSocket> downChannel;
        std::unique_ptr<AbstractStreamSocket> upChannel;
        std::tuple<ApplicationData...> requestData;

        bool isReady() const
        {
            return downChannel != nullptr && upChannel != nullptr;
        }
    };

    using Tunnels = std::map<std::string /*tunnelId*/, TunnelContext>;

    mutable QnMutex m_mutex;
    Tunnels m_tunnelsInProgress;

    virtual network::http::RequestResult processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData) override;

    void closeAllTunnels();

    void prepareCreateDownTunnelResponse(
        network::http::Response* response);

    void saveDownChannel(
        network::http::HttpServerConnection* connection,
        const std::string& tunnelId,
        ApplicationData... requestData);

    void processOpenTunnelUpChannelRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void saveUpChannel(
        network::http::HttpServerConnection* connection,
        const std::string& tunnelId);

    void reportTunnelIfReady(const std::string& tunnelId);

    bool validateOpenUpChannelRequest(
        const RequestContext& requestContext);
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
ExperimentalTunnelServer<ApplicationData...>::ExperimentalTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler))
{
}

template<typename ...ApplicationData>
ExperimentalTunnelServer<ApplicationData...>::~ExperimentalTunnelServer()
{
    closeAllTunnels();
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    using namespace std::placeholders;

    // Tunnel initiation is done by opening tunnel down channel.
    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        url::joinPath(basePath, kExperimentalTunnelDownPath),
        std::bind(&ExperimentalTunnelServer::processTunnelInitiationRequest, this, _1, _2));

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::post,
        url::joinPath(basePath, kExperimentalTunnelUpPath),
        std::bind(&ExperimentalTunnelServer::processOpenTunnelUpChannelRequest, this, _1, _2));
}

template<typename ...ApplicationData>
network::http::RequestResult
    ExperimentalTunnelServer<ApplicationData...>::processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Open experimental tunnel. Url %1")
        .args(requestContext.request.requestLine.url.path()));

    if (requestContext.requestPathParams.empty())
        return RequestResult(StatusCode::badRequest);

    RequestResult requestResult(StatusCode::ok);

    const std::string tunnelId = requestContext.requestPathParams.back();

    prepareCreateDownTunnelResponse(requestContext.response);

    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, requestData = std::make_tuple(std::move(requestData)...), tunnelId](
            HttpServerConnection* connection) mutable
        {
            auto allArgs = std::tuple_cat(
                std::make_tuple(this, connection, tunnelId),
                std::move(requestData));

            std::apply(&ExperimentalTunnelServer::saveDownChannel, std::move(allArgs));
        };

    return requestResult;
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::closeAllTunnels()
{
    Tunnels tunnelsInProgress;
    {
        QnMutexLocker lock(&m_mutex);
        tunnelsInProgress.swap(m_tunnelsInProgress);
    }

    for (auto& tunnelContext: tunnelsInProgress)
    {
        tunnelContext.second.upChannel->pleaseStopSync();
        tunnelContext.second.downChannel->pleaseStopSync();
    }
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::prepareCreateDownTunnelResponse(
    network::http::Response* response)
{
    response->headers.emplace("Content-Type", "application/octet-stream");
    response->headers.emplace("Content-Length", "10000000000");
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    response->headers.emplace("Connection", "close");
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::saveDownChannel(
    network::http::HttpServerConnection* connection,
    const std::string& tunnelId,
    ApplicationData... requestData)
{
    {
        QnMutexLocker lock(&m_mutex);

        TunnelContext& tunnelContext = m_tunnelsInProgress[tunnelId];
        // TODO: #ak For a new tunnel start some expiration timer.
        tunnelContext.downChannel = connection->takeSocket();
    }

    reportTunnelIfReady(tunnelId);
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::processOpenTunnelUpChannelRequest(
    RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!validateOpenUpChannelRequest(requestContext))
        return completionHandler(StatusCode::badRequest);

    const auto tunnelId = requestContext.requestPathParams[0].toStdString();

    network::http::RequestResult requestResult(
        nx::network::http::StatusCode::ok);

    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, tunnelId](network::http::HttpServerConnection* connection)
        {
            saveUpChannel(connection, tunnelId);
        };

    completionHandler(std::move(requestResult));
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::saveUpChannel(
    network::http::HttpServerConnection* connection,
    const std::string& tunnelId)
{
    {
        QnMutexLocker lock(&m_mutex);

        TunnelContext& tunnelContext = m_tunnelsInProgress[tunnelId];
        // TODO: #ak For a new tunnel start some expiration timer.
        tunnelContext.upChannel = connection->takeSocket();
    }

    reportTunnelIfReady(tunnelId);
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::reportTunnelIfReady(
    const std::string& tunnelId)
{
    std::unique_ptr<AbstractStreamSocket> tunnelConnection;
    TunnelContext tunnelContext;

    {
        QnMutexLocker lock(&m_mutex);

        auto tunnelContextIter = m_tunnelsInProgress.find(tunnelId);
        if (tunnelContextIter == m_tunnelsInProgress.end() ||
            !tunnelContextIter->second.isReady())
        {
            return;
        }

        tunnelContext = std::move(tunnelContextIter->second);
        m_tunnelsInProgress.erase(tunnelContextIter);
    }

    tunnelConnection = std::make_unique<SeparateUpDownChannelDelegate>(
        std::exchange(tunnelContext.upChannel, nullptr),
        std::exchange(tunnelContext.downChannel, nullptr));

    std::apply(
        &ExperimentalTunnelServer::reportTunnel,
        std::tuple_cat(
            std::make_tuple(this, std::move(tunnelConnection)),
            std::move(tunnelContext.requestData)));
}

template<typename ...ApplicationData>
bool ExperimentalTunnelServer<ApplicationData...>::validateOpenUpChannelRequest(
    const RequestContext& requestContext)
{
    if (requestContext.request.requestLine.method != network::http::Method::post ||
        requestContext.requestPathParams.empty())
    {
        return false;
    }
    // TODO

    return true;
}

} // namespace nx::network::http::tunneling::detail
