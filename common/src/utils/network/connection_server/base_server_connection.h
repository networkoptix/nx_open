/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_SERVER_CONNECTION_H
#define BASE_SERVER_CONNECTION_H

#include <forward_list>
#include <functional>
#include <memory>

#include <utils/network/abstract_socket.h>

#include "stream_socket_server.h"


namespace nx_api
{
    static const size_t READ_BUFFER_CAPACITY = 16*1024;

    //!Contains common logic for server-side connection created by \a StreamSocketServer
    /*!
        \a CustomConnectionType MUST implement following methods:
        \code {*.cpp}
            //!Received data from remote host
            void bytesReceived( const nx::Buffer& buf );
            //!Submitted data has been sent, \a m_writeBuffer has some free space
            void readyToSendData();
        \endcode

        \a CustomConnectionManagerType MUST implement following types:
        \code {*.cpp}
            //!Type of connections, handled by server. BaseServerConnection<CustomConnectionType, CustomConnectionManagerType> MUST be convertible to ConnectionType with static_cast
            typedef ... ConnectionType;
        \endcode
        and method(s):
        \code {*.cpp}
            //!This connection is passed here just after socket has been terminated
            connectionTerminated( std::shared_ptr<CustomConnectionManagerType::ConnectionType> )
        \endcode

        \todo support message interleaving
        \note This class is not thread-safe. All methods are expected to be executed in aio thread, undelying socket is bound to. 
            In other case, it is caller's responsibility to syunchronize access to the connection object.
        \note Despite absence of thread-safety simultaneous read/write operations are allowed in different threads
        \note Instance of this class MUST be handled with \a std::shared_ptr
    */
    template<
        class CustomConnectionType,
        class CustomConnectionManagerType
    > class BaseServerConnection
    :
        protected std::enable_shared_from_this<BaseServerConnection<CustomConnectionType, CustomConnectionManagerType> >
    {
    public:
        typedef BaseServerConnection<CustomConnectionType, CustomConnectionManagerType> SelfType;

        BaseServerConnection(
            CustomConnectionManagerType* connectionManager,
            AbstractCommunicatingSocket* streamSocket )
        :
            m_connectionManager( connectionManager ),
            m_streamSocket( streamSocket ),
            m_bytesToSend( 0 )
        {
            m_readBuffer.reserve( READ_BUFFER_CAPACITY );
        }

        ~BaseServerConnection()
        {
            for( auto& handler: m_connectionCloseHandlers )
                handler();
            m_connectionCloseHandlers.clear();

            m_streamSocket->cancelAsyncIO();
            m_streamSocket->close();
        }

        //!Start receiving data from connection
        /*!
            \return \a false, if could not start asynchronous operation (this can happen due to lack of resources on host machine)
        */
        bool startReadingConnection()
        {
            using namespace std::placeholders;
            return m_streamSocket->readSomeAsync( &m_readBuffer, std::bind( &SelfType::onBytesRead, this, _1, _2 ) );
        }

        /*!
            \return \a true, if started async send operation
        */
        bool sendBufAsync( const nx::Buffer& buf )
        {
            using namespace std::placeholders;
            if( !m_streamSocket->sendAsync( buf, std::bind( &SelfType::onBytesSent, this, _1, _2 ) ) )
                return false;
            m_bytesToSend = buf.size();
            return true;
        }

        void closeConnection()
        {
            m_connectionManager->connectionTerminated( std::static_pointer_cast<CustomConnectionManagerType::ConnectionType>(shared_from_this()) );
        }

        //!Register handler to be executed when connection just about to be destroyed
        /*!
            \note It is unspecified which thread \a handler will be invoked in (usually, it is aio thread corresponding to the socket)
        */
        void registerCloseHandler( std::function<void()>&& handler )
        {
            m_connectionCloseHandlers.push_back( std::move(handler) );
        }

    private:
        CustomConnectionManagerType* m_connectionManager;
        AbstractCommunicatingSocket* m_streamSocket;
        nx::Buffer m_readBuffer;
        size_t m_bytesToSend;
        std::forward_list<std::function<void()>> m_connectionCloseHandlers;

        void onBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
        {
            using namespace std::placeholders;

            if( errorCode != SystemError::noError )
                return handleSocketError( errorCode );
            if( bytesRead == 0 )    //connection closed by remote peer
                return m_connectionManager->connectionTerminated( std::static_pointer_cast<CustomConnectionManagerType::ConnectionType>(shared_from_this()) );

            assert( m_readBuffer.size() == bytesRead );
            static_cast<CustomConnectionType*>(this)->bytesReceived( m_readBuffer ); 
            m_readBuffer.resize( 0 );
            if( !m_streamSocket->readSomeAsync( &m_readBuffer, std::bind( &SelfType::onBytesRead, this, _1, _2 ) ) )
                return handleSocketError( SystemError::getLastOSErrorCode() );
        }

        void onBytesSent( SystemError::ErrorCode errorCode, size_t count )
        {
            if( errorCode != SystemError::noError )
                return handleSocketError( errorCode );

            assert( count == m_bytesToSend );

            static_cast<CustomConnectionType*>(this)->readyToSendData();
        }

        void handleSocketError( SystemError::ErrorCode errorCode )
        {
            switch( errorCode )
            {
                case SystemError::noError:
                    assert( false );
                    break;

                case SystemError::connectionReset:
                case SystemError::notConnected:
                    return m_connectionManager->connectionTerminated( std::static_pointer_cast<CustomConnectionManagerType::ConnectionType>(shared_from_this()) );

                default:
                    //TODO #ak process error code
                    return m_connectionManager->connectionTerminated( std::static_pointer_cast<CustomConnectionManagerType::ConnectionType>(shared_from_this()) );
            }
        }
    };
}

#endif  //BASE_SERVER_CONNECTION_H
