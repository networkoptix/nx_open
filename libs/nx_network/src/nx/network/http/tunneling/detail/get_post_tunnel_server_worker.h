// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <map>
#include <string>
#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffered_stream_socket.h>

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

        TunnelContext(
            std::string urlPath,
            std::tuple<ApplicationData...> requestData)
            :
            urlPath(std::move(urlPath)),
            requestData(std::move(requestData))
        {
        }
    };

    using Tunnels = std::map<int /*sequence*/, TunnelContext>;

    NewTunnelHandler m_newTunnelHandler;
    Tunnels m_tunnels;
    nx::utils::AsyncOperationGuard m_guard;
    std::atomic<int> m_tunnelSequence{0};
    std::map<HttpServerConnection*, int> m_connectionToTunnel;

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
        network::http::Response* response);

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
network::http::RequestResult
    GetPostTunnelServerWorker<ApplicationData...>::processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData)
{
    NX_VERBOSE(this, "Open GET/POST tunnel. Url %1",
        requestContext.request.requestLine.url.path());

    NX_ASSERT(this->isInSelfAioThread());

    const auto tunnelSeq = ++m_tunnelSequence;
    m_tunnels.emplace(
        tunnelSeq,
        TunnelContext(
            requestContext.request.requestLine.url.path().toStdString(),
            std::make_tuple(std::move(requestData)...)));

    if (!m_connectionToTunnel.emplace(requestContext.connection, tunnelSeq).second)
    {
        NX_DEBUG(this, "Received second GET for tunnel %1 (%2)",
            requestContext.request.requestLine.url.path(),
            requestContext.connection->lastRequestSource());
        return network::http::RequestResult(nx::network::http::StatusCode::badRequest);
    }

    requestContext.connection->registerCloseHandler(
        [this, tunnelSeq, guard = m_guard.sharedGuard(), connection = requestContext.connection](
            SystemError::ErrorCode reason, auto /*connectionDestroyed*/)
        {
            if (auto lock = guard->lock())
            {
                m_connectionToTunnel.erase(connection);

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
    requestResult.dataSource = prepareCreateDownTunnelResponse(requestContext.response);
    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, tunnelSeq](auto connection) { openDownStream(tunnelSeq, connection); };

    return requestResult;
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::stopWhileInAioThread()
{
    for (auto& [seq, tunnelContext] : m_tunnels)
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
    const auto it = m_tunnels.find(tunnelSeq);
    if (it == m_tunnels.end())
    {
        NX_ASSERT(false, "Missing tunnel context for %1", connection->lastRequestSource());
        return;
    }

    m_connectionToTunnel.erase(connection);

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

    NX_VERBOSE(this, "Opened down channel. Url %1", it->second.urlPath);

    reportTunnelIfComplete(it);
}

template<typename ...ApplicationData>
void GetPostTunnelServerWorker<ApplicationData...>::processOpenUpStreamRequest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    // It is possible that the client has sent this request early, before the GET response has been
    // sent.

    auto it = m_connectionToTunnel.find(requestContext.connection);
    if (it == m_connectionToTunnel.end())
    {
        NX_DEBUG(this, "Received unexpected POST from %1",
            requestContext.connection->lastRequestSource());
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
        NX_DEBUG(this, "Invalid up channel. Url %1", requestContext.request.requestLine.url);

        completionHandler(StatusCode::badRequest);
        return;
    }

    it->second.postReceived = true;
    it->second.postBody = std::move(requestContext.body);
    it->second.postCompletionHandler = std::move(completionHandler);

    NX_VERBOSE(this, "Received POST request. Url %1", it->second.urlPath);

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
    network::http::Response* response)
{
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    response->headers.emplace("Content-Type", "application/octet-stream");
    response->headers.emplace("Content-Length", "10000000000");
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
    SystemError::ErrorCode /*closeReason*/)
{
    m_tunnels.erase(tunnelSeq);
}

} // namespace nx::network::http::tunneling::detail
