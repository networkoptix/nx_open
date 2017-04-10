#pragma once

#include <memory>
#include <string>

#include <nx/network/abstract_socket.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

struct RelayConnectionData
{
    std::unique_ptr<AbstractStreamSocket> connection;
    std::string peerId;
};

class TrafficRelay
{
public:
    void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection);

    void terminateAllConnectionsByPeerId(const std::string& peerId);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
