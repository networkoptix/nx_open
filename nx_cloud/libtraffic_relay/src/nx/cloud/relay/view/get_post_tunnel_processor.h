#pragma once

#include <map>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/utils/thread/mutex.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

#include "../controller/connect_session_manager.h"
#include "../settings.h"

namespace nx::cloud::relay::view {

template<typename RequestSpecificData>
class GetPostTunnelProcessor:
    public network::http::StreamConnectionHolder
{
public:
    GetPostTunnelProcessor(const conf::Settings& settings);
    virtual ~GetPostTunnelProcessor();

    network::http::RequestResult processOpenTunnelRequest(
        RequestSpecificData requestData,
        const network::http::Request& request,
        network::http::Response* const response);

protected:
    virtual void onTunnelCreated(
        RequestSpecificData requestData,
        std::unique_ptr<network::AbstractStreamSocket> connection) = 0;

    void closeAllTunnels();

private:
    struct TunnelContext
    {
        RequestSpecificData requestData;
        std::string urlPath;
        std::unique_ptr<network::http::AsyncMessagePipeline> connection;
    };

    using Tunnels = std::map<network::http::AsyncMessagePipeline*, TunnelContext>;

    mutable QnMutex m_mutex;
    const conf::Settings& m_settings;
    Tunnels m_tunnelsInProgress;

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        network::http::AsyncMessagePipeline* /*connection*/) override;

    void closeConnection(
        const QnMutexLockerBase& lock,
        SystemError::ErrorCode /*closeReason*/,
        network::http::AsyncMessagePipeline* /*connection*/);

    void prepareCreateDownTunnelResponse(
        network::http::Response* const response);

    void openUpTunnel(
        network::http::HttpServerConnection* const connection,
        RequestSpecificData requestData,
        const std::string& requestPath);

    void onMessage(
        network::http::AsyncMessagePipeline* tunnel,
        network::http::Message /*httpMessage*/);

    bool validateOpenUpChannelMessage(
        const TunnelContext& tunnelContext,
        const network::http::Message& message);
};

//-------------------------------------------------------------------------------------------------

class GetPostServerTunnelProcessor:
    public GetPostTunnelProcessor<std::string /*listeningPeerName*/>
{
    using base_type = GetPostTunnelProcessor<std::string>;

public:
    GetPostServerTunnelProcessor(
        const conf::Settings& settings,
        relaying::AbstractListeningPeerPool* listeningPeerPool);
    ~GetPostServerTunnelProcessor();

protected:
    virtual void onTunnelCreated(
        std::string listeningPeerName,
        std::unique_ptr<network::AbstractStreamSocket> connection) override;

private:
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;
};

//-------------------------------------------------------------------------------------------------

class GetPostClientTunnelProcessor:
    public GetPostTunnelProcessor<
        controller::AbstractConnectSessionManager::StartRelayingFunc>
{
    using base_type = GetPostTunnelProcessor<
        controller::AbstractConnectSessionManager::StartRelayingFunc>;

public:
    GetPostClientTunnelProcessor(const conf::Settings& settings);
    ~GetPostClientTunnelProcessor();

protected:
    virtual void onTunnelCreated(
        controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc,
        std::unique_ptr<network::AbstractStreamSocket> connection) override;
};

} // namespace nx::cloud::relay::view
