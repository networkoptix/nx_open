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


/*!
    This class introduced to remove cross-includes between stun_server_connection.h and stun_stream_socket_server.h.
    Better find another solultion and remove this class
*/
template<class _ConnectionType>
    class StreamConnectionHolder
{
public:
    typedef _ConnectionType ConnectionType;

    virtual ~StreamConnectionHolder()
    {
        //we MUST be sure to remove all connections
        std::map<ConnectionType*, std::shared_ptr<ConnectionType>> connections;
        {
            std::lock_guard<std::mutex> lk( m_mutex );
            connections.swap( m_connections );
        }
        connections.clear();
    }

    void closeConnection( ConnectionType* connection )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_connections.erase( connection );
    }

protected:
    std::mutex m_mutex;
    //TODO #ak this map types seems strange. Replace with std::set?
    std::map<ConnectionType*, std::shared_ptr<ConnectionType>> m_connections;

    void saveConnection( std::shared_ptr<ConnectionType> connection )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_connections.emplace( connection.get(), std::move( connection ) );
    }
};

//!Listens local tcp address, accepts incoming connections and forwards them to the specified handler
/*!
    \uses \a aio::AIOService
*/
template<class CustomServerType, class ConnectionType>
    class StreamSocketServer
:
    public StreamConnectionHolder<ConnectionType>
{
    typedef StreamConnectionHolder<ConnectionType> BaseType;

#if defined(_MSC_VER)
    typedef typename StreamSocketServer<CustomServerType, ConnectionType> SelfType;
#else
    typedef StreamSocketServer<CustomServerType, ConnectionType> SelfType;
#endif

public:
    //!Initialization
    StreamSocketServer( bool sslRequired, SocketFactory::NatTraversalType natTraversalRequired )
    :
        m_socket( SocketFactory::createStreamServerSocket( sslRequired, natTraversalRequired ) )
    {
    }

    ~StreamSocketServer()
    {
        m_socket->cancelAsyncIO();
    }

    //!Binds to specified addresses
    /*!
        \return false on error. Use \a SystemError::getLastOSErrorCode() for error information
    */
    bool bind( const SocketAddress& socketAddress )
    {
        return 
            m_socket->setRecvTimeout( 0 ) &&
            m_socket->setReuseAddrFlag( true ) &&
            m_socket->bind( socketAddress );
    }

    //!Calls \a AbstractStreamServerSocket::listen
    bool listen()
    {
        using namespace std::placeholders;

        if( !m_socket->listen() )
            return false;
        return m_socket->acceptAsync( std::bind( &SelfType::newConnectionAccepted, this, _1, _2 ) );
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
            auto conn = createConnection( std::unique_ptr<AbstractStreamSocket>(newConnection) );
            if( conn->startReadingConnection() )
                this->saveConnection( std::move( conn ) );
        }
        if( !m_socket->acceptAsync( std::bind( &SelfType::newConnectionAccepted, this, _1, _2 ) ) )
        {
            //TODO #ak
        }
    }

protected:
    virtual std::shared_ptr<ConnectionType> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) = 0;

private:
    std::unique_ptr<AbstractStreamServerSocket> m_socket;

    StreamSocketServer( StreamSocketServer& );
    StreamSocketServer& operator=( const StreamSocketServer& );
};

#endif  //STREAM_SOCKET_SERVER_H
