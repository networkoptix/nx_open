/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/
#pragma once

#include <functional>

#include "message.h"
#include "nx/network/socket_common.h"


namespace nx {
namespace stun {

class AbstractServerConnection
{
public:
    AbstractServerConnection() {}
    virtual ~AbstractServerConnection() {}

    virtual void sendMessage(nx::stun::Message message) = 0;
    virtual nx::network::TransportProtocol transportProtocol() const = 0;
    virtual SocketAddress getSourceAddress() const = 0;
    /**
        \note \a AbstractServerConnection::sendMessage does nothing after \a handler has been invoked
    */
    virtual void addOnConnectionCloseHandler(std::function<void()> handler) = 0;

private:
    AbstractServerConnection(const AbstractServerConnection&);
    AbstractServerConnection& operator=(const AbstractServerConnection&);
};

}   //stun
}   //nx
