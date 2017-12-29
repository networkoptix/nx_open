#include "common_socket_impl.h"

namespace nx {
namespace network {

static std::atomic<SocketSequenceType> socketSequenceCounter(1);

CommonSocketImpl::CommonSocketImpl():
    aioThread(nullptr),
    terminated(0),
    socketSequence(++socketSequenceCounter),
    isUdtSocket(false)
{
}

} // namespace network
} // namespace nx
