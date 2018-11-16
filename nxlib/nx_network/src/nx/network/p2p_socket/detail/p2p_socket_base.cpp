#include "p2p_socket_base.h"
#include "p2p_socket_websocket_delegate.h"
#include "p2p_socket_http_delegate.h"
#include <nx/network/websocket/websocket.h>

namespace nx::network::detail {

void P2PSocketBase::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{

}

void P2PSocketBase::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
}

void P2PSocketBase::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
}

void P2PSocketBase::bindToAioThread(aio::AbstractAioThread* aioThread)
{
}

} // namespace nx::network::detail