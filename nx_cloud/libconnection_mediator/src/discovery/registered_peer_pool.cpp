#include "registered_peer_pool.h"

namespace nx {
namespace cloud {
namespace discovery {

RegisteredPeerPool::RegisteredPeerPool(const conf::Discovery& configuration):
    m_configuration(configuration)
{
}

void RegisteredPeerPool::addPeerConnection(
    const std::string& /*peerId*/,
    std::unique_ptr<nx::network::WebSocket> /*connection*/)
{
    // TODO
}

std::vector<std::string> RegisteredPeerPool::getPeerInfoList(
    const std::string& /*peerTypeName*/) const
{
    return std::vector<std::string>();
}

std::string RegisteredPeerPool::getPeerInfo(const std::string& /*peerId*/) const
{
    return std::string();
}

} // namespace nx
} // namespace cloud
} // namespace discovery
