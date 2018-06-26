#pragma once

#include <map>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/utils/thread/mutex.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

#include "../settings.h"

namespace nx::cloud::relay::view {

class GetPostTunnelProcessor:
    public network::http::StreamConnectionHolder
{
public:
    GetPostTunnelProcessor(
        const conf::Settings& settings,
        relaying::AbstractListeningPeerPool* listeningPeerPool);
    ~GetPostTunnelProcessor();

    network::http::RequestResult processOpenTunnelRequest(
        network::http::HttpServerConnection* const connection,
        const std::string& listeningPeerName,
        const network::http::Request& request,
        network::http::Response* const response);

private:
    struct TunnelContext
    {
        std::string listeningPeerName;
        std::string urlPath;
        std::unique_ptr<network::http::AsyncMessagePipeline> connection;
    };

    using Tunnels = std::map<network::http::AsyncMessagePipeline*, TunnelContext>;

    mutable QnMutex m_mutex;
    const conf::Settings& m_settings;
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;
    Tunnels m_tunnelsInProgress;

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        network::http::AsyncMessagePipeline* /*connection*/) override;

    void prepareCreateDownTunnelResponse(
        network::http::Response* const response);

    void openUpTunnel(
        network::http::HttpServerConnection* const connection,
        const std::string& listeningPeerName,
        const std::string& requestPath);

    void onMessage(
        Tunnels::iterator tunnelIter,
        network::http::Message /*httpMessage*/);

    bool validateOpenUpChannelMessage(
        const TunnelContext& tunnelContext,
        const network::http::Message& message);
};

} // namespace nx::cloud::relay::view
