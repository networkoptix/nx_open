#ifndef nx_cc_cloud_server_socket_h
#define nx_cc_cloud_server_socket_h

#include "../abstract_socket.h"

namespace nx {
namespace network {
namespace cloud {

//!Accepts connections incoming via mediator
/*!
    Listening hostname is reported to the mediator to listen on.
    \todo #ak what listening port should mean in this case?
*/
class NX_NETWORK_API CloudServerSocket
:
    public AbstractStreamServerSocket
{
public:
    //!Implementation of AbstractSocket::bind
    /*!
        \param endpoint Host address is reported to the mediator to listen on. port ???
    */
    virtual bool bind( const SocketAddress& endpoint ) override;

    //!Implementation of AbstractStreamServerSocket::listen
    virtual bool listen( int queueLen ) override;
    //!Implementation of AbstractStreamServerSocket::accept
    virtual AbstractStreamSocket* accept() override;
    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop( std::function< void() > handler ) override;

protected:
    //!Implementation of AbstractStreamServerSocket::acceptAsync
    virtual void acceptAsync(
        std::function<void(
            SystemError::ErrorCode,
            AbstractStreamSocket* )> handler ) override;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif  //nx_cc_cloud_server_socket_h
