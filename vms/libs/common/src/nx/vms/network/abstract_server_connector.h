#pragma once

#include <chrono>

#include <network/router.h>
#include <nx/network/abstract_socket.h>

namespace nx {
namespace vms {
namespace network {

class AbstractServerConnector
{
public:
    virtual ~AbstractServerConnector() {}
    /*
     * Open connection to the media server defined by QnUuid.
     */
    virtual std::unique_ptr<nx::network::AbstractStreamSocket> connectTo(
        const QnRoute& route,
        bool sslRequired,
        std::chrono::milliseconds timeout) = 0;
};

} // namespace network
} //namespace vms
} // namespace nx
