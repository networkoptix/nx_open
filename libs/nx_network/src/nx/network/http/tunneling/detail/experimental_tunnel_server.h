// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/aio/repetitive_timer.h>
#include <nx/network/debug/object_instance_counter.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "../../empty_message_body_source.h"
#include "../../http_types.h"
#include "../../server/http_server_connection.h"
#include "../../server/http_stream_socket_server.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"
#include "../abstract_tunnel_authorizer.h"
#include "basic_custom_tunnel_server.h"
#include "request_paths.h"
#include "separate_up_down_channel_delegate.h"

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
        debug::ObjectInstanceCounter<TunnelContext> objectInstanceCounter;

        bool isReady() const
        {
            return downChannel != nullptr && upChannel != nullptr;
        }
    };

    using Tunnels = std::map<std::string /*tunnelId*/, TunnelContext>;

    mutable nx::Mutex m_mutex;
    Tunnels m_tunnelsInProgress;
    nx::utils::ElapsedTimerPool<std::string /*tunnelId*/> m_inactiveConnectionTimerPool;
    aio::RepetitiveTimer m_removeInactiveTunnelsTimer;

    virtual network::http::RequestResult processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData) override;

    std::unique_ptr<AbstractMsgBodySource> prepareCreateDownTunnelResponse(
        network::http::HttpHeaders* responseHeaders);

    void saveDownChannel(
        network::http::HttpServerConnection* connection,
        const std::string& tunnelId,
        ApplicationData... requestData);

    void saveUpChannel(
        network::http::HttpServerConnection* connection,
        const std::string& tunnelId);

    void reportTunnelIfReady(const std::string& tunnelId);

    void startIdleTimeout(
        const std::string& tunnelId,
        std::chrono::milliseconds timeout);

    void dropTunnel(const std::string& tunnelId);

    bool validateOpenUpChannelRequest(
        const RequestContext& requestContext);
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
SeparateUpDownConnectionsTunnelServer<ApplicationData...>::SeparateUpDownConnectionsTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler)),
    m_inactiveConnectionTimerPool(
        [this](const auto& tunnelId) { dropTunnel(tunnelId); })
{
    static constexpr auto kCheckForInactiveTunnelsPeriod = std::chrono::seconds(1);

    m_removeInactiveTunnelsTimer.start(
        kCheckForInactiveTunnelsPeriod,
        [this]()
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_inactiveConnectionTimerPool.processTimers();
        });
}

template<typename ...ApplicationData>
SeparateUpDownConnectionsTunnelServer<ApplicationData...>::~SeparateUpDownConnectionsTunnelServer()
{
    m_removeInactiveTunnelsTimer.pleaseStopSync();

    closeAllTunnels();
}

template<typename ...ApplicationData>
network::http::RequestResult
    SeparateUpDownConnectionsTunnelServer<ApplicationData...>::processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData)
{
    NX_VERBOSE(this, "Open experimental tunnel. Url %1",
        requestContext.request.requestLine.url.path());

    if (requestContext.requestPathParams.empty())
        return RequestResult(StatusCode::badRequest);

    RequestResult requestResult(StatusCode::ok);

    const std::string tunnelId = requestContext.requestPathParams.getByName(kTunnelIdName);

    requestResult.body = prepareCreateDownTunnelResponse(&requestResult.headers);

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
        NX_MUTEX_LOCKER lock(&m_mutex);
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
        network::http::HttpHeaders* responseHeaders)
{
    responseHeaders->emplace("Cache-Control", "no-store");
    responseHeaders->emplace("Pragma", "no-cache");

    return std::make_unique<EmptyMessageBodySource>(
        "application/octet-stream",
        10'000'000'000ULL);
}

template<typename ...ApplicationData>
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::saveDownChannel(
    network::http::HttpServerConnection* connection,
    const std::string& tunnelId,
    ApplicationData... requestData)
{
    if (connection->pendingRequestCount() > 0 || connection->pendingResponseCount() > 0)
    {
        NX_VERBOSE(this, "Cannot save down channel from %1 since there are unexpected "
            "pipelined request(s). Closing connection...",
            connection->lastRequestSource());
        connection->closeConnection(SystemError::connectionReset);
        return;
    }

    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto [it, inserted] = m_tunnelsInProgress.emplace(tunnelId, TunnelContext());
        if (inserted && connection->inactivityTimeout())
            startIdleTimeout(tunnelId, *connection->inactivityTimeout());

        it->second.downChannel = connection->takeSocket();
        it->second.requestData = std::make_tuple(std::move(requestData)...);
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
    if (connection->pendingRequestCount() > 0 || connection->pendingResponseCount() > 0)
    {
        NX_VERBOSE(this, "Cannot save up channel from %1 since there are unexpected "
            "pipelined request(s). Closing connection...",
            connection->lastRequestSource());
        connection->closeConnection(SystemError::connectionReset);
        return;
    }

    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto [it, inserted] = m_tunnelsInProgress.emplace(tunnelId, TunnelContext());
        if (inserted && connection->inactivityTimeout())
            startIdleTimeout(tunnelId, *connection->inactivityTimeout());

        it->second.upChannel = connection->takeSocket();
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
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto tunnelContextIter = m_tunnelsInProgress.find(tunnelId);
        if (tunnelContextIter == m_tunnelsInProgress.end() ||
            !tunnelContextIter->second.isReady())
        {
            return;
        }

        m_inactiveConnectionTimerPool.removeTimer(tunnelId);

        tunnelContext = std::move(tunnelContextIter->second);
        m_tunnelsInProgress.erase(tunnelContextIter);
    }

    tunnelConnection = std::make_unique<SeparateUpDownChannelDelegate>(
        std::exchange(tunnelContext.upChannel, nullptr),
        std::exchange(tunnelContext.downChannel, nullptr),
        SeparateUpDownChannelDelegate::Purpose::server);

    std::apply(
        &SeparateUpDownConnectionsTunnelServer::reportTunnel,
        std::tuple_cat(
            std::make_tuple(this, std::move(tunnelConnection)),
            std::move(tunnelContext.requestData)));
}

template<typename ...ApplicationData>
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::startIdleTimeout(
    const std::string& tunnelId,
    std::chrono::milliseconds timeout)
{
    m_inactiveConnectionTimerPool.addTimer(tunnelId, timeout);
}

template<typename ...ApplicationData>
void SeparateUpDownConnectionsTunnelServer<ApplicationData...>::dropTunnel(
    const std::string& tunnelId)
{
    m_tunnelsInProgress.erase(tunnelId);
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

    // TODO: #akolesnikov Looks like the validation should be stronger.

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
        typename base_type::NewTunnelHandler newTunnelHandler)
        :
        base_type(std::move(newTunnelHandler))
    {
    }

    virtual ~ExperimentalTunnelServer();

    virtual void registerRequestHandlers(
        const std::string& basePath,
        AbstractMessageDispatcher* messageDispatcher) override;
};

template<typename ...ApplicationData>
ExperimentalTunnelServer<ApplicationData...>::~ExperimentalTunnelServer()
{
    this->closeAllTunnels();
}

template<typename ...ApplicationData>
void ExperimentalTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    AbstractMessageDispatcher* messageDispatcher)
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
