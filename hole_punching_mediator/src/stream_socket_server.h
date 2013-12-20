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
        //m_handler.reset( new CustomHandlerHolder<HandlerType>( handler ) );
        if( !m_socket->listen() )
            return false;
        return m_socket->acceptAsync( std::bind( std::mem_fn(&SelfType::newConnectionAccepted), this ) );
    }

    SocketAddress address() const
    {
        return m_socket->getLocalAddress();
    }

    void newConnectionAccepted(
        SystemError::ErrorCode /*errorCode*/,
        AbstractStreamSocket* newConnection )
    {
        //(*m_handler)( errorCode, newConnection );
        if( newConnection )
        {
            auto conn = std::make_shared<ConnectionType>( static_cast<CustomServerType*>(this), newConnection );
            std::unique_lock<std::mutex> lk( m_mutex );
            m_connections.insert( conn );
        }
        m_socket->acceptAsync( this );
    }

    void connectionTerminated( const ConnectionPtr& connection )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        m_connections.erase( connection );
    }

private:
    //class HandlerHolder
    //{
    //public:
    //    virtual ~HandlerHolder() {}

    //    virtual void operator()(
    //        SystemError::ErrorCode errorCode,
    //        AbstractStreamSocket* newConnection ) = 0;
    //};

    //template<class HandlerType>
    //    class CustomHandlerHolder
    //:
    //    public HandlerHolder
    //{
    //public:
    //    CustomHandlerHolder( const HandlerType& handler )
    //    :
    //        m_handler( handler )
    //    {
    //    }

    //    virtual void operator()(
    //        SystemError::ErrorCode errorCode,
    //        AbstractStreamSocket* newConnection ) override
    //    {
    //        m_handler( errorCode, newConnection );
    //    }

    //private:
    //    HandlerType m_handler;
    //};

    std::shared_ptr<AbstractStreamServerSocket> m_socket;
    std::mutex m_mutex;
    std::set<ConnectionPtr> m_connections;
    //std::unique_ptr<HandlerHolder> m_handler;
};

#endif  //STREAM_SOCKET_SERVER_H
