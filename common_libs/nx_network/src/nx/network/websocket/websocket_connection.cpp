#include <nx/network/websocket/websocket_connection.h>

namespace nx {
namespace network {
namespace websocket {

Connection::Connection(
    StreamConnectionHolder<Connection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> sock) 
    :
    BaseType(socketServer, std::move(sock))
{

}

void Connection::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{

}

void Connection::processMessage(Message&& request)
{
}

}
}
}