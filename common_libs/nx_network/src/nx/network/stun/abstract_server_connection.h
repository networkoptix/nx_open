/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/
#pragma once

#include <functional>

#include <utils/common/systemerror.h>

#include "message.h"
#include "nx/network/socket_common.h"
#include "nx/network/abstract_socket.h"


namespace nx {
namespace stun {

class AbstractServerConnection
{
public:
    AbstractServerConnection() {}
    virtual ~AbstractServerConnection() {}

    /**
        \param handler Triggered to report send result. Can be \a NULL
    */
    virtual void sendMessage(
        nx::stun::Message message,
        std::function<void(SystemError::ErrorCode)> handler = nullptr) = 0;
    virtual nx::network::TransportProtocol transportProtocol() const = 0;
    virtual SocketAddress getSourceAddress() const = 0;
    /**
        \note \a AbstractServerConnection::sendMessage does nothing after \a handler has been invoked
    */
    virtual void addOnConnectionCloseHandler(std::function<void()> handler) = 0;
    virtual AbstractCommunicatingSocket* socket() = 0;

private:
    AbstractServerConnection(const AbstractServerConnection&);
    AbstractServerConnection& operator=(const AbstractServerConnection&);
};

}   //stun
}   //nx
