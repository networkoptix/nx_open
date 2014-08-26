/**********************************************************
* 18 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef STREAM_SOCKET_SERVER_H
#define STREAM_SOCKET_SERVER_H

#include <functional>
#include <memory>
#include <mutex>
#include <set>

#include <utils/network/abstract_socket.h>
#include <utils/network/socket_factory.h>
#include <utils/network/socket_common.h>


//!Listens local tcp address, accepts incoming connections and forwards them to the specified handler
/*!
    \uses \a aio::AIOService
*/
template<class CustomServerType, class ConnectionType>
    class StreamSocketServer
{
public:
    typedef typename StreamSocketServer<CustomServerType, ConnectionType> SelfType;
    typedef typename std::shared_ptr<ConnectionType> ConnectionPtr;

    //!Initialization
    StreamSocketServer()
    :
        m_socket( SocketFactory::createStreamServerSocket() )
    {
        m_socket->setRecvTimeout( 0 );
    }

    ~StreamSocketServer()
    {
        //TODO/IMPL
    }

    //!Binds to specified addresses
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() for error information
    */
    bool bind( const SocketAddress& socketAddress )
    {
        return m_socket->bind( socketAddress );
    }

    //!Calls \a AbstractStreamServerSocket::listen
    //template<class HandlerType>
    //    bool listen( const HandlerType& handler )
    bool listen()
    {
        using namespace std::placeholders;

        if( !m_socket->listen() )
            return false;
        return m_socket->acceptAsync( std::bind( std::mem_fn(&SelfType::newConnectionAccepted), this, _1, _2 ) );
    }

    SocketAddress address() const
    {
        return m_socket->getLocalAddress();
    }

    void newConnectionAccepted(
        SystemError::ErrorCode /*errorCode*/,
        AbstractStreamSocket* newConnection )
    {
        using namespace std::placeholders;

        if( newConnection )
        {
            auto conn = std::make_shared<ConnectionType>( static_cast<CustomServerType*>(this), newConnection );
            std::unique_lock<std::mutex> lk( m_mutex );
            m_connections.insert( conn );
        }
        m_socket->acceptAsync( std::bind( std::mem_fn(&SelfType::newConnectionAccepted), this, _1, _2 ) );
    }

    void connectionTerminated( const ConnectionPtr& connection )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_connections.erase( connection );
    }

private:
    std::shared_ptr<AbstractStreamServerSocket> m_socket;
    std::mutex m_mutex;
    std::set<ConnectionPtr> m_connections;
};

#endif  //STREAM_SOCKET_SERVER_H
