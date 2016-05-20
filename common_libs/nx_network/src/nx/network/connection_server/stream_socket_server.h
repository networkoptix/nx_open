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

#include <utils/common/stoppable.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_common.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>


template<class _ConnectionType>
class StreamConnectionHolder
{
public:
    typedef _ConnectionType ConnectionType;

    virtual ~StreamConnectionHolder() {}

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection ) = 0;
};

template<class _ConnectionType>
class StreamServerConnectionHolder
    : public StreamConnectionHolder<_ConnectionType>
{
public:
    typedef _ConnectionType ConnectionType;

    StreamServerConnectionHolder()
    :
        m_connectionsBeingClosedCount(0)
    {
    }

    virtual ~StreamServerConnectionHolder()
    {
        //we MUST be sure to remove all connections
        std::map<ConnectionType*, std::shared_ptr<ConnectionType>> connections;
        {
            QnMutexLocker lk(&m_mutex);
            connections.swap( m_connections );
        }
        for (auto& connection: connections)
            connection.first->pleaseStopSync();
        connections.clear();

        //waiting connections being cancelled through closeConnection call to finish...
        QnMutexLocker lk(&m_mutex);
        while (m_connectionsBeingClosedCount > 0)
            m_cond.wait(lk.mutex());
    }

    virtual void closeConnection(
        SystemError::ErrorCode /*closeReason*/,
        ConnectionType* connection ) override
    {
        QnMutexLocker lk(&m_mutex);
        auto connectionIter = m_connections.find(connection);
        if (connectionIter == m_connections.end())
            return;
        auto connectionCtx = std::move(*connectionIter);
        m_connections.erase(connectionIter);

        ++m_connectionsBeingClosedCount;
        lk.unlock();

        //we are in connection's aio thread, so we can just free connection
        connectionCtx.second.reset();

        lk.relock();
        --m_connectionsBeingClosedCount;
        m_cond.wakeAll();
    }

protected:
    void saveConnection(std::shared_ptr<ConnectionType> connection)
    {
        QnMutexLocker lk(&m_mutex);
        m_connections.emplace(connection.get(), std::move(connection));
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    int m_connectionsBeingClosedCount;
    //TODO #ak this map types seems strange. Replace with std::set?
    std::map<ConnectionType*, std::shared_ptr<ConnectionType>> m_connections;
};

//!Listens local tcp address, accepts incoming connections and forwards them to the specified handler
/*!
    \uses \a aio::AIOService
*/
template<class CustomServerType, class ConnectionType>
    class StreamSocketServer
:
    public StreamServerConnectionHolder<ConnectionType>,
    public QnStoppable
{
    typedef StreamServerConnectionHolder<ConnectionType> BaseType;

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
        pleaseStop();
    }

    virtual void pleaseStop()
    {
        m_socket->pleaseStopSync();
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
        m_socket->acceptAsync( std::bind( &SelfType::newConnectionAccepted, this, _1, _2 ) );
        return true;
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

        //TODO #ak handle errorCode: try to call acceptAsync after some delay?

        if( newConnection )
        {
            auto conn = createConnection( std::unique_ptr<AbstractStreamSocket>(newConnection) );
            conn->startReadingConnection();
            this->saveConnection(std::move(conn));
        }
        m_socket->acceptAsync(std::bind(&SelfType::newConnectionAccepted, this, _1, _2));
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
