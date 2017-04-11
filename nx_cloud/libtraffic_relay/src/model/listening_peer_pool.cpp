#include "listening_peer_pool.h"

namespace nx {
namespace cloud {
namespace relay {
namespace model {

void ListeningPeerPool::addConnection(
    const std::string& /*peerName*/,
    std::unique_ptr<AbstractStreamSocket> /*connection*/)
{
    // TODO
}

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
