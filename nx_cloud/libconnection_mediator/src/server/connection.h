/**********************************************************
* Dec 25, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_CONNECTION_H
#define NX_MEDIATOR_CONNECTION_H

#include <functional>
#include <memory>

#include <nx/network/stun/message.h>
#include <nx/network/stun/server_connection.h>
#include <nx/network/socket_common.h>


namespace nx {
namespace hpm {

#if 0

/**
    \note Life time of object of this class is tied to the life time 
        of \a nx::stun::ServerConnection instance
    \note Class methods are not thread-safe!
 */
class TcpServerConnection
:
    public AbstractServerConnection,
    public std::enable_shared_from_this<TcpServerConnection>
{
public:
    virtual ~TcpServerConnection();

    virtual void sendMessage(nx::stun::Message message) override;
    virtual nx::network::TransportProtocol transportProtocol() const override;
    virtual SocketAddress getSourceAddress() const override;
    virtual void registerCloseHandler(std::function<void()> handler) override;

    static std::shared_ptr<TcpServerConnection> create(
        std::shared_ptr< nx::stun::ServerConnection > connection);

private:
    std::weak_ptr< nx::stun::ServerConnection > m_connection;
    std::function<void()> m_connectionTerminatedHandler;

    TcpServerConnection();

    void setConnection(std::shared_ptr< nx::stun::ServerConnection > connection);
    void connectionIsAboutToClose(
        std::shared_ptr<TcpServerConnection> strongThisRef);
};
#endif

/** Provides ability to send response to a request message received via UDP
 */
class UDPServerConnection   //TODO #ak rename
:
    public nx::stun::AbstractServerConnection
{
public:
};

}   //hpm
}   //nx

#endif  //NX_MEDIATOR_CONNECTION_H
