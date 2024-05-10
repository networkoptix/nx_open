// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <map>
#include <string>
#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/buffered_stream_socket.h>
#include <nx/network/debug/object_instance_counter.h>
#include <nx/utils/log/log.h>

#include "../../async_message_pipeline.h"
#include "../../server/http_server_connection.h"

namespace nx::network::http::tunneling::detail {

/**
 * A worker that is used by GetPostTunnelServer internally.
 * See GetPostTunnelServer for the description of GET/POST HTTP tunnel.
 */
template<typename ...ApplicationData>
class GetPostTunnelServerWorker:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using NewTunnelHandler = nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<AbstractStreamSocket> connection,
        ApplicationData...)>;

    GetPostTunnelServerWorker(NewTunnelHandler newTunnelHandler);
    virtual ~GetPostTunnelServerWorker();

    virtual void bindToAioThread(aio::AbstractAioThread*) override;

    void setTunnelSetupTimeout(std::chrono::milliseconds timeout);

    /**
     * Processes the initial (GET) request. This request opens down stream.
     */
    network::http::RequestResult processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData);

    void processOpenUpStreamRequest(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct TunnelContext
    {
        std::string urlPath;
        std::tuple<ApplicationData...> requestData;
        bool getResponseSent = false;
        bool postReceived = false;
        std::unique_ptr<AsyncMessagePipeline> connection;
        std::unique_ptr<AbstractMsgBodySourceWithCache> postBody;
        RequestProcessedHandler postCompletionHandler;
        std::unique_ptr<nx::network::aio::Timer> timer;

        TunnelContext(
            std::string urlPath,
            std::tuple<ApplicationData...> requestData)
            :
            urlPath(std::move(urlPath)),
            requestData(std::move(requestData))
        {
        }

    private:
        debug::ObjectInstanceCounter<TunnelContext> m_objectInstanceCounter;
    };

    using Tunnels = std::map<int /*sequence*/, TunnelContext>;

    NewTunnelHandler m_newTunnelHandler;
    Tunnels m_tunnels;
    nx::utils::AsyncOperationGuard m_guard;
    std::atomic<int> m_tunnelSequence{0};
    std::map<std::uint64_t /*connectionId*/, int> m_connectionToTunnel;
    std::chrono::milliseconds m_timeout{std::chrono::seconds(15)};

    void closeConnection(
        int tunnelSeq,
        SystemError::ErrorCode /*closeReason*/);

    void openDownStream(int tunnelSeq, HttpServerConnection* connection);

    void processOpenUpStreamRequest(
        int tunnelSeq,
        RequestContext requestContext,
        RequestProcessedHandler completionHandler);

    void processOpenUpStreamMessage(
        int tunnelSeq,
        network::http::Message message);

    void reportTunnelIfComplete(typename Tunnels::iterator it);

    std::unique_ptr<AbstractMsgBodySource> prepareCreateDownTunnelResponse(
        network::http::HttpHeaders* responseHeaders);

    bool validateOpenUpChannelRequest(
        const TunnelContext& tunnelContext,
        const network::http::Request& request);
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
GetPostTunnelServerWorker<ApplicationData...>::GetPostTunnelServerWorker(
    NewTunnelHandler newTunnelHandler)
    :
    m_newTunnelHandler(std::move(newTunnelHandler))
{
}

template<typename ...ApplicationData>
GetPostTunnelServerWorker<ApplicationData...>::~GetPostTunnelServerWorker()
{
    m_guard->terminate();

    pleaseStopSync();
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    if (aioThread == getAioThread())
        return;

    // The worker is not expected to be re-bound to a different AIO thread.
    // That's breaks the idea of this class and is not supported.
    NX_CRITICAL(false, "GetPostTunnelServerWorker cannot be re-bound to a different AIO thread");
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::setTunnelSetupTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

template<typename ...ApplicationData>
network::http::RequestResult
    GetPostTunnelServerWorker<ApplicationData...>::processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData)
{
    NX_ASSERT(this->isInSelfAioThread());

    auto connection = requestContext.conn.lock();
    if (!connection)
    {
        NX_VERBOSE(this, "Connection already closed. %1 (%2)",
            requestContext.request.requestLine.url.path(),
            requestContext.clientEndpoint);
        return network::http::RequestResult(StatusCode::internalServerError);
    }

    NX_ASSERT(connection->isInSelfAioThread());

    const auto tunnelSeq = ++m_tunnelSequence;
    NX_VERBOSE(this, "%1. Open GET/POST tunnel. Url %2. Tunnel count %3", tunnelSeq,
        requestContext.request.requestLine.url.path(), m_tunnels.size());

    if (!m_connectionToTunnel.emplace(requestContext.connectionAttrs.id, tunnelSeq).second)
    {
        NX_DEBUG(this, "%1. Received second GET for tunnel %2 (%3)", tunnelSeq,
            requestContext.request.requestLine.url.path(),
            requestContext.clientEndpoint);
        return network::http::RequestResult(StatusCode::badRequest);
    }

    auto [tunnelIter, inserted] = m_tunnels.emplace(
        tunnelSeq,
        TunnelContext(
            requestContext.request.requestLine.url.path().toStdString(),
            std::make_tuple(std::move(requestData)...)));
    if (!NX_ASSERT(inserted, "non-unique tunnelSeq (%1) was generated. m_tunnels.size() = %2",
            tunnelSeq, m_tunnels.size()))
    {
        m_connectionToTunnel.erase(requestContext.connectionAttrs.id);
        return network::http::RequestResult(StatusCode::internalServerError);
    }

    tunnelIter->second.timer = std::make_unique<nx::network::aio::Timer>();
    tunnelIter->second.timer->start(
        m_timeout,
        [this, tunnelSeq]()
        {
            NX_VERBOSE(this, "%1. Tunnel timed out", tunnelSeq);
            closeConnection(tunnelSeq, SystemError::timedOut);
        });

    connection->registerCloseHandler(
        [this, tunnelSeq, guard = m_guard.sharedGuard(),
            connectionId = requestContext.connectionAttrs.id](
                SystemError::ErrorCode reason, auto /*connectionDestroyed*/)
        {
            if (auto lock = guard->lock())
            {
                NX_VERBOSE(this, "%1. Tunnel HTTP connection closed", tunnelSeq);
                NX_ASSERT(this->isInSelfAioThread());

                m_connectionToTunnel.erase(connectionId);

                const auto it = m_tunnels.find(tunnelSeq);
                if (it == m_tunnels.end())
                    return;

                // NOTE: HttpServerConnection does not support removing close connection handler.
                // This condition replaces it.
                if (it->second.getResponseSent)
                    return;

                closeConnection(tunnelSeq, reason);
            }
        });

    network::http::RequestResult requestResult(nx::network::http::StatusCode::ok);
    requestResult.body = prepareCreateDownTunnelResponse(&requestResult.headers);
    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, tunnelSeq](auto connection) { openDownStream(tunnelSeq, connection); };

    return requestResult;
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::stopWhileInAioThread()
{
    for (auto& [seq, tunnelContext]: m_tunnels)
    {
        if (tunnelContext.postCompletionHandler)
            std::exchange(tunnelContext.postCompletionHandler, nullptr)(StatusCode::ok);
    }
    m_tunnels.clear();

    m_connectionToTunnel.clear();
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::openDownStream(
    int tunnelSeq,
    HttpServerConnection* connection)
{
    NX_ASSERT(this->isInSelfAioThread());

    const auto it = m_tunnels.find(tunnelSeq);
    if (it == m_tunnels.end())
    {
        NX_ASSERT(false, "%1. Missing tunnel context for %2", tunnelSeq, connection->lastRequestSource());
        return;
    }

    m_connectionToTunnel.erase(connection->attrs().id);

    // Receiving the POST message here so that it does not require an authentication.
    it->second.connection = std::make_unique<AsyncMessagePipeline>(
        connection->takeSocket());
    it->second.getResponseSent = true;

    if (!it->second.postReceived)
    {
        // Receiving POST request.
        it->second.connection->registerCloseHandler(
            [this, tunnelSeq](auto closeReason, auto /*connectionDestroyed*/)
            {
                closeConnection(tunnelSeq, closeReason);
            });
        it->second.connection->setMessageHandler(
            [this, tunnelSeq](auto&&... args)
            {
                this->processOpenUpStreamMessage(
                    tunnelSeq, std::forward<decltype(args)>(args)...);
            });
        it->second.connection->startReadingConnection(connection->inactivityTimeout());
    }

    NX_VERBOSE(this, "%1. Opened down channel. Url %2", tunnelSeq, it->second.urlPath);

    reportTunnelIfComplete(it);
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::processOpenUpStreamRequest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    // It is possible that the client has sent this request early, before the GET response has been
    // sent.
    NX_ASSERT(this->isInSelfAioThread());

    auto it = m_connectionToTunnel.find(requestContext.connectionAttrs.id);
    if (it == m_connectionToTunnel.end())
    {
        NX_DEBUG(this, "Received unexpected POST from %1", requestContext.clientEndpoint);
        return completionHandler(StatusCode::badRequest);
    }

    auto tunnelSeq = it->second;
    m_connectionToTunnel.erase(it);

    processOpenUpStreamRequest(
        tunnelSeq,
        std::move(requestContext),
        std::move(completionHandler));
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::processOpenUpStreamRequest(
    int tunnelSeq,
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    auto it = m_tunnels.find(tunnelSeq);
    if (it == m_tunnels.end() ||
        !validateOpenUpChannelRequest(it->second, requestContext.request))
    {
        NX_DEBUG(this, "%1. Invalid up channel. Url %2", tunnelSeq,
            requestContext.request.requestLine.url);

        completionHandler(StatusCode::badRequest);
        return;
    }

    it->second.postReceived = true;
    it->second.postBody = std::move(requestContext.body);
    it->second.postCompletionHandler = std::move(completionHandler);

    NX_VERBOSE(this, "%1. Received POST request. Url %2", tunnelSeq, it->second.urlPath);

    reportTunnelIfComplete(it);
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::processOpenUpStreamMessage(
    int tunnelSeq,
    network::http::Message message)
{
    if (message.type != MessageType::request)
        return;

    RequestContext requestContext;
    requestContext.request = std::move(*message.request);
    processOpenUpStreamRequest(
        tunnelSeq,
        std::move(requestContext),
        [this, tunnelSeq](RequestResult result)
    {
        if (!StatusCode::isSuccessCode(result.statusCode))
            closeConnection(tunnelSeq, SystemError::invalidData);
    });
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::reportTunnelIfComplete(
    typename Tunnels::iterator it)
{
    if (!it->second.getResponseSent || !it->second.postReceived)
        return; //< The tunnel is not completed yet.

    NX_VERBOSE(this, "%1. Tunnel completed", it->first);

    auto tunnelContext = std::move(it->second);
    m_tunnels.erase(it);

    auto socket = tunnelContext.connection->takeSocket();

    if (tunnelContext.postBody)
    {
        if (auto peekResult = tunnelContext.postBody->peek(); peekResult)
        {
            auto& [resultCode, buffer] = *peekResult;
            if (!buffer.empty())
            {
                socket = std::make_unique<BufferedStreamSocket>(
                    std::move(socket), std::move(buffer));
            }
        }
    }

    std::apply(
        m_newTunnelHandler,
        std::tuple_cat(
            std::make_tuple(std::move(socket)),
            std::move(tunnelContext.requestData)));

    if (tunnelContext.postCompletionHandler)
    {
        // NOTE: This response will not be delivered after connection->takeSocket() call.
        // But, it is needed to destroy the request context.
        std::exchange(tunnelContext.postCompletionHandler, nullptr)(StatusCode::ok);
    }
}

template<typename ...ApplicationData>
std::unique_ptr<AbstractMsgBodySource>
GetPostTunnelServerWorker<ApplicationData...>::prepareCreateDownTunnelResponse(
    network::http::HttpHeaders* responseHeaders)
{
    responseHeaders->emplace("Cache-Control", "no-store");
    responseHeaders->emplace("Pragma", "no-cache");
    responseHeaders->emplace("Content-Type", "application/octet-stream");
    responseHeaders->emplace("Content-Length", "10000000000");
    return nullptr;
}

template<typename ...ApplicationData>
bool GetPostTunnelServerWorker<ApplicationData...>::validateOpenUpChannelRequest(
    const TunnelContext& tunnelContext,
    const network::http::Request& request)
{
    return request.requestLine.method == network::http::Method::post
        && request.requestLine.url.path().toStdString() == tunnelContext.urlPath;
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::closeConnection(
    int tunnelSeq,
    SystemError::ErrorCode closeReason)
{
    NX_ASSERT(this->isInSelfAioThread());

    NX_VERBOSE(this, "%1. Closing tunnel with result %2. Tunnel count %3",
        tunnelSeq, closeReason, m_tunnels.size());

    m_tunnels.erase(tunnelSeq);
}

} // namespace nx::network::http::tunneling::detail
