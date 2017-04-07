#pragma once

#include <memory>
#include <string>

#include <nx/network/abstract_socket.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

class ListeningPeerPool
{
public:
    void addConnection(
        const std::string& peerName,
        std::unique_ptr<AbstractStreamSocket> connection);
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
