#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "basic_custom_tunnel_server.h"
#include "../abstract_tunnel_authorizer.h"
#include "../../http_types.h"
#include "../../server/http_server_connection.h"
#include "../../server/http_stream_socket_server.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling::detail {

template<typename ApplicationData>
class GetPostTunnelServer:
    public BasicCustomTunnelServer<ApplicationData>,
    public network::http::StreamConnectionHolder
{
    using base_type = BasicCustomTunnelServer<ApplicationData>;

public:
    GetPostTunnelServer(typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~GetPostTunnelServer();

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

private:
    struct TunnelContext
    {
        ApplicationData requestData;
        std::string urlPath;
        std::unique_ptr<network::http::AsyncMessagePipeline> connection;
    };

    using Tunnels = std::map<network::http::AsyncMessagePipeline*, TunnelContext>;

    mutable QnMutex m_mutex;
    Tunnels m_tunnelsInProgress;

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        network::http::AsyncMessagePipeline* /*connection*/) override;

    virtual network::http::RequestResult processOpenTunnelRequest(
        ApplicationData requestData,
        const network::http::Request& request,
        network::http::Response* response) override;

    void closeAllTunnels();

    void closeConnection(
        const QnMutexLockerBase& lock,
        SystemError::ErrorCode /*closeReason*/,
        network::http::AsyncMessagePipeline* /*connection*/);

    void prepareCreateDownTunnelResponse(
        network::http::Response* response);

    void openUpTunnel(
        network::http::HttpServerConnection* connection,
        ApplicationData requestData,
        const std::string& requestPath);

    void onMessage(
        network::http::AsyncMessagePipeline* tunnel,
        network::http::Message /*httpMessage*/);

    bool validateOpenUpChannelMessage(
        const TunnelContext& tunnelContext,
        const network::http::Message& message);
};

//-------------------------------------------------------------------------------------------------

template<typename ApplicationData>
GetPostTunnelServer<ApplicationData>::GetPostTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler))
{
}

template<typename ApplicationData>
GetPostTunnelServer<ApplicationData>::~GetPostTunnelServer()
{
    closeAllTunnels();
}

template<typename ApplicationData>
void GetPostTunnelServer<ApplicationData>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    using namespace std::placeholders;

    static constexpr char path[] = "get_post/{sequence}";

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        !this->requestPath().empty() ? this->requestPath() : url::joinPath(basePath, path),
        std::bind(&GetPostTunnelServer::processTunnelInitiationRequest, this, _1, _2));
}

template<typename ApplicationData>
network::http::RequestResult
    GetPostTunnelServer<ApplicationData>::processOpenTunnelRequest(
        ApplicationData requestData,
        const network::http::Request& request,
        network::http::Response* response)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Open GET/POST tunnel. Url %1")
        .args(request.requestLine.url.path()));

    network::http::RequestResult requestResult(
        nx::network::http::StatusCode::ok);

    prepareCreateDownTunnelResponse(response);

    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, requestData = std::move(requestData),
            requestPath = request.requestLine.url.path().toStdString()](
                network::http::HttpServerConnection* connection) mutable
        {
            openUpTunnel(
                connection,
                std::move(requestData),
                requestPath);
        };

    return requestResult;
}

template<typename ApplicationData>
void GetPostTunnelServer<ApplicationData>::closeAllTunnels()
{
    Tunnels tunnelsInProgress;
    {
        QnMutexLocker lock(&m_mutex);
        tunnelsInProgress.swap(m_tunnelsInProgress);
    }

    for (auto& tunnelContext : tunnelsInProgress)
        tunnelContext.second.connection->pleaseStopSync();
}

template<typename ApplicationData>
void GetPostTunnelServer<ApplicationData>::prepareCreateDownTunnelResponse(
    network::http::Response* response)
{
    response->headers.emplace("Content-Type", "application/octet-stream");
    response->headers.emplace("Content-Length", "10000000000");
    response->headers.emplace("Cache-Control", "no-store");
    response->headers.emplace("Pragma", "no-cache");
    response->headers.emplace("Connection", "close");
}

template<typename ApplicationData>
void GetPostTunnelServer<ApplicationData>::openUpTunnel(
    network::http::HttpServerConnection* connection,
    ApplicationData requestData,
    const std::string& requestPath)
{
    using namespace std::placeholders;

    auto httpPipe = std::make_unique<network::http::AsyncMessagePipeline>(
        this,
        connection->takeSocket());
    auto httpPipePtr = httpPipe.get();

    QnMutexLocker lock(&m_mutex);

    auto insertionResult = m_tunnelsInProgress.emplace(
        httpPipePtr,
        TunnelContext{std::move(requestData), requestPath, std::move(httpPipe)});

    insertionResult.first->second.connection->setMessageHandler(
        std::bind(&GetPostTunnelServer::onMessage, this, httpPipePtr, _1));
    insertionResult.first->second.connection->startReadingConnection();
}

template<typename ApplicationData>
void GetPostTunnelServer<ApplicationData>::onMessage(
    network::http::AsyncMessagePipeline* tunnel,
    network::http::Message message)
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

    this->reportTunnel(
        std::move(tunnelContext.requestData),
        tunnelContext.connection->takeSocket());
}

template<typename ApplicationData>
bool GetPostTunnelServer<ApplicationData>::validateOpenUpChannelMessage(
    const TunnelContext& tunnelContext,
    const network::http::Message& message)
{
    if (message.type != network::http::MessageType::request ||
        message.request->requestLine.method != network::http::Method::post ||
        message.request->requestLine.url.path().toStdString() != tunnelContext.urlPath)
    {
        return false;
    }

    return true;
}

template<typename ApplicationData>
void GetPostTunnelServer<ApplicationData>::closeConnection(
    SystemError::ErrorCode closeReason,
    network::http::AsyncMessagePipeline* connection)
{
    QnMutexLocker lock(&m_mutex);
    closeConnection(lock, closeReason, connection);
}

template<typename ApplicationData>
void GetPostTunnelServer<ApplicationData>::closeConnection(
    const QnMutexLockerBase& /*lock*/,
    SystemError::ErrorCode /*closeReason*/,
    network::http::AsyncMessagePipeline* connection)
{
    m_tunnelsInProgress.erase(connection);
}

} // namespace nx::network::http::tunneling::detail
