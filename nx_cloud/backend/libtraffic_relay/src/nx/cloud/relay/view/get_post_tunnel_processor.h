#pragma once

#include <map>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/tunneling/get_post_tunnel_processor.h>
#include <nx/utils/thread/mutex.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

#include "../controller/connect_session_manager.h"

namespace nx::cloud::relay::view {

class GetPostServerTunnelProcessor:
    public network::http::tunneling::GetPostTunnelProcessor<std::string /*listeningPeerName*/>
{
    using base_type = network::http::tunneling::GetPostTunnelProcessor<std::string>;

public:
    GetPostServerTunnelProcessor(
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
    public network::http::tunneling::GetPostTunnelProcessor<
        controller::AbstractConnectSessionManager::StartRelayingFunc>
{
    using base_type = network::http::tunneling::GetPostTunnelProcessor<
        controller::AbstractConnectSessionManager::StartRelayingFunc>;

public:
    ~GetPostClientTunnelProcessor();

protected:
    virtual void onTunnelCreated(
        controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc,
        std::unique_ptr<network::AbstractStreamSocket> connection) override;
};

} // namespace nx::cloud::relay::view
