/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_SERVER_CONNECTION_H
#define BASE_SERVER_CONNECTION_H

#include <functional>
#include <memory>

#include <utils/network/abstract_socket.h>

#include "stream_socket_server.h"


//!Contains common logic for server-side connection created by \a StreamSocketServer
/*!
    \a CustomConnectionType MUST define following methods:
    \code {*.cpp}
        //!Received data from remote host
        void bytesReceived( const nx::Buffer& buf );
        //!Submitted data has been sent, \a m_writeBuffer has some free space
        void readyToSendData();
    \endcode

    \todo support message interleaving
*/
template<
    class CustomConnectionType,
    class CustomSocketServerType
> class BaseServerConnection
:
    std::enable_shared_from_this<BaseServerConnection<CustomConnectionType, CustomSocketServerType> >
{
public:
    typedef BaseServerConnection<CustomConnectionType, CustomSocketServerType> SelfType;

    BaseServerConnection(
        CustomSocketServerType* streamServer,
        AbstractCommunicatingSocket* streamSocket )
    :
        m_streamServer( streamServer ),
        m_streamSocket( streamSocket ),
        m_bytesToSend( 0 )
    {
        using namespace std::placeholders;
        m_streamSocket->readSomeAsync( &m_readBuffer, std::bind( std::mem_fn( &SelfType::onBytesRead ), this, _1, _2 ) );
    }

    void sendBufAsync( const nx::Buffer& buf )
    {
        using namespace std::placeholders;
        m_streamSocket->sendAsync( buf, std::bind( std::mem_fn( &SelfType::onBytesSent ), this, _1, _2 ) );
        m_bytesToSend = buf.size();
    }

private:
    CustomSocketServerType* m_streamServer;
    AbstractCommunicatingSocket* m_streamSocket;
    nx::Buffer m_readBuffer;
    size_t m_bytesToSend;

    void onBytesRead( SystemError::ErrorCode errorCode, size_t count )
    {
        using namespace std::placeholders;

        if( errorCode != SystemError::noError )
            return handleSocketError( errorCode );

        static_cast<CustomConnectionType*>(this)->bytesReceived( m_readBuffer.substr(0, count) );
        m_streamSocket->readSomeAsync( &m_readBuffer, std::bind( std::mem_fn( &SelfType::onBytesRead ), this, _1, _2 ) );
    }

    void onBytesSent( SystemError::ErrorCode errorCode, size_t count )
    {
        if( errorCode != SystemError::noError )
            return handleSocketError( errorCode );

        assert( count == m_bytesToSend );

        //m_writeBuffer.pop_front(count);
        //if( !m_writeBuffer.empty() )
        //    return sendBufAsync( m_writeBuffer );
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
                return m_streamServer->connectionTerminated( std::static_pointer_cast<CustomSocketServerType::ConnectionType>(shared_from_this()) );

            default:
                //TODO/IMPL #ak process error code
                return m_streamServer->connectionTerminated( std::static_pointer_cast<CustomSocketServerType::ConnectionType>(shared_from_this()) );
        }
    }
};

#endif  //BASE_SERVER_CONNECTION_H
