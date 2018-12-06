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
class SeparateUpDownConnectionsTunnelServer:
    public BasicCustomTunnelServer<ApplicationData...>
{
    using base_type = BasicCustomTunnelServer<ApplicationData...>;

public:
    SeparateUpDownConnectionsTunnelServer(typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~SeparateUpDownConnectionsTunnelServer();

protected:
    void processOpenTunnelUpChannelRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void closeAllTunnels();

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

    std::unique_ptr<AbstractMsgBodySource> prepareCreateDownTunnelResponse(
        network::http::Response* response);

    void saveDownChannel(
        network::http::HttpServerConnection* connection,
        const std::string& tunnelId,
        ApplicationData... requestData);

    void saveUpChannel(
        network::http::HttpServerConnection* connection,
        const std::string& tunnelId);

    void reportTunnelIfReady(const std::string& tunnelId);

    bool validateOpenUpChannelRequest(
        const RequestContext& requestContext);
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
SeparateUpDownConnectionsTunnelServer<ApplicationData...>::SeparateUpDownConnectionsTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler))
{
}

template<typename ...ApplicationData>
SeparateUpDownConnectionsTunnelServer<ApplicationData...>::~SeparateUpDownConnectionsTunnelServer()
{
    closeAllTunnels();
}

template<typename ...ApplicationData>
network::http::RequestResult
    SeparateUpDownConnectionsTunnelServer<ApplicationData...>::processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Open experimental tunnel. Url %1")
        .args(requestContext.request.requestLine.url.path()));

    if (requestContext.requestPathParams.empty())
        return RequestResult(StatusCode::badRequest);

    RequestResult requestResult(StatusCode::ok);

    const std::string tunnelId = requestContext.requestPathParams.getByName(kTunnelIdName);

    requestResult.dataSource =
        prepareCreateDownTunnelResponse(requestContext.response);

    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, requestData = std::make_tuple(std::move(requestData)...), tunnelId](
            HttpServerConnection* connection) mutable
        {
            auto allArgs = std::tuple_cat(
                std::make_tuple(this, connection, tunnelId),
                std::move(requestData));

            std::apply(
                &SeparateUpDownConnectionsTunnelServer::saveDownChannel,
                std::move(allArgs));
        };

    return requestResult;
}

template<typename ...ApplicationData>
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::closeAllTunnels()
{
    Tunnels tunnelsInProgress;
    {
        QnMutexLocker lock(&m_mutex);
        tunnelsInProgress.swap(m_tunnelsInProgress);
    }

    for (auto& tunnelContext: tunnelsInProgress)
    {
        if (tunnelContext.second.upChannel)
            tunnelContext.second.upChannel->pleaseStopSync();
        if (tunnelContext.second.downChannel)
            tunnelContext.second.downChannel->pleaseStopSync();
    }
}

template<typename ...ApplicationData>
std::unique_ptr<AbstractMsgBodySource>
    SeparateUpDownConnectionsTunnelServer<ApplicationData...>::prepareCreateDownTunnelResponse(
        network::http::Response* response)
{
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    return std::make_unique<EmptyMessageBodySource>(
        "application/octet-stream",
        10000000000ULL);
}

template<typename ...ApplicationData>
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::saveDownChannel(
    network::http::HttpServerConnection* connection,
    const std::string& tunnelId,
    ApplicationData... requestData)
{
    {
        QnMutexLocker lock(&m_mutex);

        TunnelContext& tunnelContext = m_tunnelsInProgress[tunnelId];
        // TODO: #ak For a new tunnel start some expiration timer.
        tunnelContext.downChannel = connection->takeSocket();
        tunnelContext.requestData = std::make_tuple(std::move(requestData)...);
    }

    reportTunnelIfReady(tunnelId);
}

template<typename ...ApplicationData>
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::processOpenTunnelUpChannelRequest(
    RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!validateOpenUpChannelRequest(requestContext))
        return completionHandler(StatusCode::badRequest);

    const auto tunnelId = requestContext.requestPathParams.getByName(kTunnelIdName);

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
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::saveUpChannel(
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
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::reportTunnelIfReady(
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
        &SeparateUpDownConnectionsTunnelServer::reportTunnel,
        std::tuple_cat(
            std::make_tuple(this, std::move(tunnelConnection)),
            std::move(tunnelContext.requestData)));
}

template<typename ...ApplicationData>
bool SeparateUpDownConnectionsTunnelServer<ApplicationData...>::validateOpenUpChannelRequest(
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

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
class ExperimentalTunnelServer:
    public SeparateUpDownConnectionsTunnelServer<ApplicationData...>
{
    using base_type = SeparateUpDownConnectionsTunnelServer<ApplicationData...>;

public:
    ExperimentalTunnelServer(
        typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~ExperimentalTunnelServer();

    virtual void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher) override;
};

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
    this->closeAllTunnels();
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    // Tunnel initiation is done by opening tunnel down channel.
    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        url::joinPath(basePath, kExperimentalTunnelDownPath),
        [this](auto&&... args) { this->processTunnelInitiationRequest(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::post,
        url::joinPath(basePath, kExperimentalTunnelUpPath),
        [this](auto&&... args) { this->processOpenTunnelUpChannelRequest(std::move(args)...); });
}

} // namespace nx::network::http::tunneling::detail
