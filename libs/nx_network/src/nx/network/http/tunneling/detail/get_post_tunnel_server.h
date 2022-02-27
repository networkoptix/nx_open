// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/debug/object_instance_counter.h>
#include <nx/network/http/async_message_pipeline.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "basic_custom_tunnel_server.h"
#include "request_paths.h"
#include "../abstract_tunnel_authorizer.h"
#include "../../http_types.h"
#include "../../server/http_server_connection.h"
#include "../../server/http_stream_socket_server.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling::detail {

/**
 * Accepts HTTP tunnel based on subsequent GET/POST requests.
 * Example:
 * <pre><code>
 * C->S:
 * GET /get_post/1 HTTP/1.1
 *
 * S->C:
 * HTTP/1.1 200 OK
 * Content-Length: 10000000000
 * </code></pre>
 *
 * C->S:
 * POST /get_post/1 HTTP/1.1
 * Content-Length: 10000000000
 *
 * After receiving POST request, server uses TCP connection as a binary tunnel.
 * The client is expected to use it as a tunnel after receiving response to GET and sending POST.
 */
template<typename ...ApplicationData>
class GetPostTunnelServer:
    public BasicCustomTunnelServer<ApplicationData...>
{
    using base_type = BasicCustomTunnelServer<ApplicationData...>;

public:
    GetPostTunnelServer(typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~GetPostTunnelServer();

    virtual void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher) override;

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
        debug::ObjectInstanceCounter<TunnelContext> objectInstanceCounter;

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

    mutable nx::Mutex m_mutex;
    Tunnels m_tunnels;
    nx::utils::AsyncOperationGuard m_guard;
    std::atomic<int> m_tunnelSequence{0};
    std::map<HttpServerConnection*, int> m_connectionToTunnel;

    void closeConnection(
        int tunnelSeq,
        SystemError::ErrorCode /*closeReason*/);

    /**
     * Processes the initial (GET) request. This request opens down stream.
     */
    virtual network::http::RequestResult processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData) override;

    void openDownStream(int tunnelSeq, HttpServerConnection* connection);

    void processOpenUpStreamRequest(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler);

    void processOpenUpStreamRequest(
        int tunnelSeq,
        RequestContext requestContext,
        RequestProcessedHandler completionHandler);

    void processOpenUpStreamMessage(
        int tunnelSeq,
        network::http::Message message);

    void reportTunnelIfComplete(
        nx::Locker<nx::Mutex>* lock,
        typename Tunnels::iterator it);

    void closeAllTunnels();

    std::unique_ptr<AbstractMsgBodySource> prepareCreateDownTunnelResponse(
        network::http::Response* response);

    bool validateOpenUpChannelRequest(
        const TunnelContext& tunnelContext,
        const network::http::Request& request);
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
GetPostTunnelServer<ApplicationData...>::GetPostTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler))
{
}

template<typename ...ApplicationData>
GetPostTunnelServer<ApplicationData...>::~GetPostTunnelServer()
{
    closeAllTunnels();
    m_guard->terminate();
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    const auto path = this->requestPath().empty()
        ? url::joinPath(basePath, kGetPostTunnelPath)
        : this->requestPath();

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        path,
        [this](auto&&... args)
        {
            this->processTunnelInitiationRequest(std::forward<decltype(args)>(args)...);
        });

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::post,
        path,
        [this](auto&&... args)
        {
            this->processOpenUpStreamRequest(std::forward<decltype(args)>(args)...);
        },
        MessageBodyDeliveryType::stream);
}

template<typename ...ApplicationData>
network::http::RequestResult
    GetPostTunnelServer<ApplicationData...>::processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData)
{
    NX_VERBOSE(this, "Open GET/POST tunnel. Url %1",
        requestContext.request.requestLine.url.path());

    NX_MUTEX_LOCKER lock(&m_mutex);

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
            SystemError::ErrorCode reason)
        {
            if (auto lock = guard->lock())
            {
                NX_MUTEX_LOCKER mtxLock(&m_mutex);

                m_connectionToTunnel.erase(connection);

                const auto it = m_tunnels.find(tunnelSeq);
                if (it == m_tunnels.end())
                    return;

                // NOTE: HttpServerConnection does not support removing close connection handler.
                // This condition replaces it.
                if (it->second.getResponseSent)
                    return;

                mtxLock.unlock();

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
void GetPostTunnelServer<ApplicationData...>::openDownStream(
    int tunnelSeq,
    HttpServerConnection* connection)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

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
            [this, tunnelSeq](auto closeReason) { closeConnection(tunnelSeq, closeReason); });
        it->second.connection->setMessageHandler(
            [this, tunnelSeq](auto&&... args)
            {
                this->processOpenUpStreamMessage(
                    tunnelSeq, std::forward<decltype(args)>(args)...);
            });
        it->second.connection->startReadingConnection(connection->inactivityTimeout());
    }

    NX_VERBOSE(this, "Opened down channel. Url %1", it->second.urlPath);

    reportTunnelIfComplete(&lock, it);
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::processOpenUpStreamRequest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    // It is possible that the client has sent this request early, before the GET response has been
    // sent.

    NX_MUTEX_LOCKER lock(&m_mutex);

    auto it = m_connectionToTunnel.find(requestContext.connection);
    if (it == m_connectionToTunnel.end())
    {
        lock.unlock();

        NX_DEBUG(this, "Received unexpected POST from %1",
            requestContext.connection->lastRequestSource());
        return completionHandler(StatusCode::badRequest);
    }

    auto tunnelSeq = it->second;
    m_connectionToTunnel.erase(it);

    lock.unlock();

    processOpenUpStreamRequest(
        tunnelSeq,
        std::move(requestContext),
        std::move(completionHandler));
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::processOpenUpStreamRequest(
    int tunnelSeq,
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto it = m_tunnels.find(tunnelSeq);
    if (it == m_tunnels.end() ||
        !validateOpenUpChannelRequest(it->second, requestContext.request))
    {
        NX_DEBUG(this, "Invalid up channel. Url %1", requestContext.request.requestLine.url);
        lock.unlock();

        completionHandler(StatusCode::badRequest);
        return;
    }

    it->second.postReceived = true;
    it->second.postBody = std::move(requestContext.body);
    it->second.postCompletionHandler = std::move(completionHandler);

    NX_VERBOSE(this, "Received POST request. Url %1", it->second.urlPath);

    reportTunnelIfComplete(&lock, it);
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::processOpenUpStreamMessage(
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
void GetPostTunnelServer<ApplicationData...>::reportTunnelIfComplete(
    nx::Locker<nx::Mutex>* lock,
    typename Tunnels::iterator it)
{
    if (!it->second.getResponseSent || !it->second.postReceived)
        return; //< The tunnel is not completed yet.

    auto tunnelContext = std::move(it->second);
    m_tunnels.erase(it);

    nx::Unlocker<nx::Mutex> unlocker(lock);

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
        &GetPostTunnelServer::reportTunnel,
        std::tuple_cat(
            std::make_tuple(this, std::move(socket)),
            std::move(tunnelContext.requestData)));

    if (tunnelContext.postCompletionHandler)
    {
        // NOTE: This response will not be delivered after connection->takeSocket() call.
        // But, it is needed to destroy the request context.
        std::exchange(tunnelContext.postCompletionHandler, nullptr)(StatusCode::ok);
    }
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::closeAllTunnels()
{
    Tunnels tunnelsInProgress;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        tunnelsInProgress.swap(m_tunnels);
    }

    for (auto& [seq, tunnelContext]: tunnelsInProgress)
    {
        if (tunnelContext.postCompletionHandler)
            std::exchange(tunnelContext.postCompletionHandler, nullptr)(StatusCode::ok);

        if (tunnelContext.connection)
            tunnelContext.connection->pleaseStopSync();
    }
}

template<typename ...ApplicationData>
std::unique_ptr<AbstractMsgBodySource>
    GetPostTunnelServer<ApplicationData...>::prepareCreateDownTunnelResponse(
        network::http::Response* response)
{
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    response->headers.emplace("Content-Type", "application/octet-stream");
    response->headers.emplace("Content-Length", "10000000000");
    return nullptr;
}

template<typename ...ApplicationData>
bool GetPostTunnelServer<ApplicationData...>::validateOpenUpChannelRequest(
    const TunnelContext& tunnelContext,
    const network::http::Request& request)
{
    return request.requestLine.method == network::http::Method::post
        && request.requestLine.url.path().toStdString() == tunnelContext.urlPath;
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::closeConnection(
    int tunnelSeq,
    SystemError::ErrorCode /*closeReason*/)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_tunnels.erase(tunnelSeq);
}

} // namespace nx::network::http::tunneling::detail
