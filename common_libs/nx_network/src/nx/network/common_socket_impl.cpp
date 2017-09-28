#include "common_socket_impl.h"

static std::atomic<SocketSequenceType> socketSequenceCounter(1);

CommonSocketImpl::CommonSocketImpl():
    aioThread(nullptr),
    terminated(0),
    socketSequence(++socketSequenceCounter),
    isUdtSocket(false)
{
}
