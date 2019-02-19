#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "basic_custom_tunnel_server.h"
#include "request_paths.h"
#include "../abstract_tunnel_authorizer.h"
#include "../../http_types.h"
#include "../../server/http_server_connection.h"
#include "../../server/http_stream_socket_server.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"

namespace nx_http::tunneling::detail {

template<typename ...ApplicationData>
class GetPostTunnelServer:
    public BasicCustomTunnelServer<ApplicationData...>,
    public nx_http::StreamConnectionHolder
{
    using base_type = BasicCustomTunnelServer<ApplicationData...>;

public:
    GetPostTunnelServer(typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~GetPostTunnelServer();

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

private:
    struct TunnelContext
    {
        std::string urlPath;
        std::unique_ptr<nx_http::AsyncMessagePipeline> connection;
        std::tuple<ApplicationData...> requestData;
    };

    using Tunnels = std::map<nx_http::AsyncMessagePipeline*, TunnelContext>;

    mutable QnMutex m_mutex;
    Tunnels m_tunnelsInProgress;

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        nx_http::AsyncMessagePipeline* /*connection*/) override;

    virtual nx_http::RequestResult processOpenTunnelRequest(
        const nx_http::Request& request,
        nx_http::Response* response,
        ApplicationData... requestData) override;

    void closeAllTunnels();

    void closeConnection(
        const QnMutexLockerBase& lock,
        SystemError::ErrorCode /*closeReason*/,
        nx_http::AsyncMessagePipeline* /*connection*/);

    void prepareCreateDownTunnelResponse(
        nx_http::Response* response);

    void openUpTunnel(
        nx_http::HttpServerConnection* connection,
        const std::string& requestPath,
        ApplicationData... requestData);

    void onMessage(
        nx_http::AsyncMessagePipeline* tunnel,
        nx_http::Message /*httpMessage*/);

    bool validateOpenUpChannelMessage(
        const TunnelContext& tunnelContext,
        const nx_http::Message& message);
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
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    using namespace std::placeholders;

    const auto path = this->requestPath().empty()
        ? url::joinPath(basePath, kGetPostTunnelPath)
        : this->requestPath();

    messageDispatcher->registerRequestProcessorFunc(
        nx_http::Method::get,
        path,
        std::bind(&GetPostTunnelServer::processTunnelInitiationRequest, this, _1, _2));
}

template<typename ...ApplicationData>
nx_http::RequestResult
    GetPostTunnelServer<ApplicationData...>::processOpenTunnelRequest(
        const nx_http::Request& request,
        nx_http::Response* response,
        ApplicationData... requestData)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Open GET/POST tunnel. Url %1")
        .args(request.requestLine.url.path()));

    nx_http::RequestResult requestResult(
        nx_http::StatusCode::ok);

    prepareCreateDownTunnelResponse(response);

    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, requestData = std::make_tuple(std::move(requestData)...),
            requestPath = request.requestLine.url.path().toStdString()](
                nx_http::HttpServerConnection* connection) mutable
        {
            auto allArgs = std::tuple_cat(
                std::make_tuple(this, connection, requestPath),
                std::move(requestData));

            std::apply(&GetPostTunnelServer::openUpTunnel, std::move(allArgs));
        };

    return requestResult;
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::closeAllTunnels()
{
    Tunnels tunnelsInProgress;
    {
        QnMutexLocker lock(&m_mutex);
        tunnelsInProgress.swap(m_tunnelsInProgress);
    }

    for (auto& tunnelContext : tunnelsInProgress)
        tunnelContext.second.connection->pleaseStopSync();
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::prepareCreateDownTunnelResponse(
    nx_http::Response* response)
{
    response->headers.emplace("Content-Type", "application/octet-stream");
    response->headers.emplace("Content-Length", "10000000000");
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    response->headers.emplace("Connection", "close");
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::openUpTunnel(
    nx_http::HttpServerConnection* connection,
    const std::string& requestPath,
    ApplicationData... requestData)
{
    using namespace std::placeholders;

    auto httpPipe = std::make_unique<nx_http::AsyncMessagePipeline>(
        this,
        connection->takeSocket());
    auto httpPipePtr = httpPipe.get();

    QnMutexLocker lock(&m_mutex);

    auto insertionResult = m_tunnelsInProgress.emplace(
        httpPipePtr,
        TunnelContext{
            requestPath,
            std::move(httpPipe),
            std::make_tuple(std::move(requestData)...)});

    insertionResult.first->second.connection->setMessageHandler(
        std::bind(&GetPostTunnelServer::onMessage, this, httpPipePtr, _1));
    insertionResult.first->second.connection->startReadingConnection();
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::onMessage(
    nx_http::AsyncMessagePipeline* tunnel,
    nx_http::Message message)
{
    QnMutexLocker lock(&m_mutex);

    auto tunnelIter = m_tunnelsInProgress.find(tunnel);
    if (tunnelIter == m_tunnelsInProgress.end())
        return; //< GetPostTunnelServer is being destroyed.

    if (!validateOpenUpChannelMessage(tunnelIter->second, message))
    {
        NX_DEBUG(this, lm("Invalid up channel. Url %1")
            .args(tunnelIter->second.urlPath));
        closeConnection(lock, SystemError::invalidData, tunnelIter->first);
        return;
    }

    auto tunnelContext = std::move(tunnelIter->second);
    m_tunnelsInProgress.erase(tunnelIter);

    NX_VERBOSE(this, lm("Successfully opened GET/POST tunnel. Url %1")
        .args(tunnelContext.urlPath));

    std::apply(
        &GetPostTunnelServer::reportTunnel,
        std::tuple_cat(
            std::make_tuple(this, tunnelContext.connection->takeSocket()),
            std::move(tunnelContext.requestData)));
}

template<typename ...ApplicationData>
bool GetPostTunnelServer<ApplicationData...>::validateOpenUpChannelMessage(
    const TunnelContext& tunnelContext,
    const nx_http::Message& message)
{
    if (message.type != nx_http::MessageType::request ||
        message.request->requestLine.method != nx_http::Method::post ||
        message.request->requestLine.url.path().toStdString() != tunnelContext.urlPath)
    {
        return false;
    }

    return true;
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::closeConnection(
    SystemError::ErrorCode closeReason,
    nx_http::AsyncMessagePipeline* connection)
{
    QnMutexLocker lock(&m_mutex);
    closeConnection(lock, closeReason, connection);
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::closeConnection(
    const QnMutexLockerBase& /*lock*/,
    SystemError::ErrorCode /*closeReason*/,
    nx_http::AsyncMessagePipeline* connection)
{
    m_tunnelsInProgress.erase(connection);
}

} // namespace nx_http::tunneling::detail
