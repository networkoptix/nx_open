#include "common_socket_impl.h"

namespace nx {
namespace network {

static std::atomic<SocketSequenceType> socketSequenceCounter(1);

CommonSocketImpl::CommonSocketImpl():
    socketSequence(++socketSequenceCounter)
{
}

} // namespace network
} // namespace nx
