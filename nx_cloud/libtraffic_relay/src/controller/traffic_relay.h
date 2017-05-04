#pragma once

#include <memory>
#include <string>

#include <nx/network/abstract_socket.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

struct RelayConnectionData
{
    std::unique_ptr<AbstractStreamSocket> connection;
    std::string peerId;
};

class AbstractTrafficRelay
{
public:
    virtual ~AbstractTrafficRelay() = default;

    virtual void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection) = 0;
};

class TrafficRelay:
    public AbstractTrafficRelay
{
public:
    virtual void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection) override;

    void terminateAllConnectionsByPeerId(const std::string& peerId);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
