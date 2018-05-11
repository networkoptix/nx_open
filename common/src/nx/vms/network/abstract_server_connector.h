#pragma once

#include <network/router.h>

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
    virtual std::unique_ptr<nx::network::AbstractStreamSocket> connect(
        const QnRoute& route, std::chrono::milliseconds timeout) = 0;
};

} // namespace network
} //namespace vms
} // namespace nx
