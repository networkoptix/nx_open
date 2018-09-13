#include "get_post_tunnel_processor.h"

#include <nx/utils/log/log.h>

#include <nx/cloud/relaying/listening_peer_manager.h>

namespace nx::cloud::relay::view {

GetPostServerTunnelProcessor::GetPostServerTunnelProcessor(
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    m_listeningPeerPool(listeningPeerPool)
{
}

GetPostServerTunnelProcessor::~GetPostServerTunnelProcessor()
{
    closeAllTunnels();
}

void GetPostServerTunnelProcessor::onTunnelCreated(
    std::string listeningPeerName,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    m_listeningPeerPool->addConnection(
        listeningPeerName,
        std::move(connection));
}

//-------------------------------------------------------------------------------------------------

GetPostClientTunnelProcessor::~GetPostClientTunnelProcessor()
{
    closeAllTunnels();
}

void GetPostClientTunnelProcessor::onTunnelCreated(
    controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    startRelayingFunc(std::move(connection));
}

} // namespace nx::cloud::relay::view
