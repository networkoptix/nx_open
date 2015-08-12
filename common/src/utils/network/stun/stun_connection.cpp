/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#include "stun_connection.h"

namespace nx {
namespace stun {

    ClientConnection::ClientConnection(
        const SocketAddress& stunServerEndpoint,
        bool useSsl )
    :
        m_stunServerEndpoint( stunServerEndpoint ),
        m_state(NOT_CONNECTED)
    {
        m_socket.reset( SocketFactory::createStreamSocket( useSsl, SocketFactory::nttDisabled ) );
        m_socket->setNonBlockingMode(true);
    }

    void ClientConnection::pleaseStop( std::function<void()>&& completionHandler )
    {
        // Cancel the Pending IO operations
        if( m_state == NOT_CONNECTED )
        {
            if( completionHandler )
                completionHandler();
            return;
        }

        //TODO #ak pass completionHandler to m_socket->cancelAsyncIO
        //m_socket->cancelAsyncIO();
        m_baseConnection->closeConnection();
        m_outstandingRequest.reset();
        if( m_state.load(std::memory_order_acquire) != CONNECTED ) {
            m_state = NOT_CONNECTED;
        }
        if( completionHandler )
            completionHandler();
    }

    void ClientConnection::enqueuePendingRequest( Message&& msg , std::function<void(SystemError::ErrorCode,Message&&)>&& func ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_ASSERT(!m_outstandingRequest);
        m_outstandingRequest.reset( new PendingRequest(std::move(msg),std::move(func)) );
    }

    void ClientConnection::resetOutstandingRequest() {
        if( !m_outstandingRequest )
            return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outstandingRequest.reset();
    }

    void ClientConnection::onConnectionComplete(
        SystemError::ErrorCode ec , 
        const std::function<void(SystemError::ErrorCode)>& completion_handler )
    {
        if( ec )
        {
            m_state.store(NOT_CONNECTED,std::memory_order_release);
            return;
        }
        else
        {
            m_state.store(CONNECTED,std::memory_order_release);
        }

        m_baseConnection.reset( new BaseConnectionType( this, std::move(m_socket) ) );
        using namespace std::placeholders;
        m_baseConnection->setMessageHandler(
            [this]( Message&& msg ) {
                processMessage( std::move( msg ) );
            } );

        // Set up the read connection. If this cannot be setup we treat it
        // as cannot finish the connection .
        if( ec == SystemError::noError && !m_baseConnection->startReadingConnection() ) {
            ec = STUN_CONN_CANNOT_READ;
        }

        if( completion_handler )
            completion_handler(ec);

        bool ret = dispatchPendingRequest(ec);
        if( !ret ) {
            if( m_outstandingRequest ) {
                m_outstandingRequest->completion_handler(
                    SystemError::getLastOSErrorCode(),Message());
                resetOutstandingRequest();
            }
        }
    }

    void ClientConnection::onRequestSend( SystemError::ErrorCode ec ) {
        if( ec ) {
            m_outstandingRequest->completion_handler(ec,Message());
            resetOutstandingRequest();
        }
    }

    // This function will _NOT_ remove pending outstanding IO event , so
    // the caller should call resetOustandingRequest per to the semantic 

    bool ClientConnection::dispatchPendingRequest( SystemError::ErrorCode ec ) {
        // Checking if we have any pending operations
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if( !m_outstandingRequest || m_outstandingRequest->execute )
                return false;
            m_outstandingRequest->execute = true;
        }
        if( ec ) {
            m_outstandingRequest->completion_handler(ec,Message());
            return false;
        }
        // Dequeue the request from the pending request queue
        return m_baseConnection->sendMessage( 
            Message(std::move(m_outstandingRequest->request_message)),
            std::bind(
                &ClientConnection::onRequestSend,
                this,
                std::placeholders::_1));
    }

    bool ClientConnection::openConnection( std::function<void(SystemError::ErrorCode)>&& completionHandler )
    {
        Q_ASSERT( m_state == NOT_CONNECTED );
        m_state = CONNECTING;
        return m_socket->connectAsync(
            m_stunServerEndpoint,
            std::bind(
            &ClientConnection::onConnectionComplete,
                this,
                std::placeholders::_1,
                std::move(completionHandler) ) );
    }

    void ClientConnection::registerIndicationHandler( std::function<void(Message&&)>&& indicationHandler )
    {
        m_indicationHandler = indicationHandler;
    }

    bool ClientConnection::sendRequest(
        Message&& request,
        std::function<void(SystemError::ErrorCode, Message&&)>&& completionHandler )
    {
        Q_ASSERT( request.header.messageClass == MessageClass::request );
        enqueuePendingRequest(
            std::move( request ),
            std::move( completionHandler ) );

        switch(m_state.load(std::memory_order_acquire)) {
            case NOT_CONNECTED:
                return openConnection( [ this ]( SystemError::ErrorCode code )
                {
                    if( code != SystemError::noError )
                    {
                        m_outstandingRequest->completion_handler( code, Message() );
                        resetOutstandingRequest();
                    }
                } );

            case CONNECTING:
                return true;

            case CONNECTED:
                if( !dispatchPendingRequest( SystemError::noError ) ) {
                    // The send error will be treated as an error happened locally so no user's
                    // handler is invoking , otherwise potentially recursive lock can happen .
                    resetOutstandingRequest();
                    return false;
                }
                return true;

            default: Q_ASSERT(0);
                return false;
        }
    }

    void ClientConnection::closeConnection( BaseConnectionType* connection )
    {
        assert( connection == m_baseConnection.get() );
        if( m_socket )
            m_socket->close();
    }

    void ClientConnection::onRequestMessageRecv( Message&& msg )
    {
        // Checking the message to see whether it is an valid message or not
        if( msg.header.messageClass == MessageClass::errorResponse ) {
            if( !hasErrorAttribute((msg) ) ) {
                m_outstandingRequest->completion_handler(STUN_REPLY_PACKAGE_BROKEN,std::move(msg));
            } else {
                m_outstandingRequest->completion_handler(0,std::move(msg));
            }
        } else {
            m_outstandingRequest->completion_handler(0,std::move(msg));
        }
        // This message chain processing is finished here
        resetOutstandingRequest();
    }

    void ClientConnection::onIndicationMessageRecv( Message&& msg )
    {
        if( m_indicationHandler )
            m_indicationHandler(std::move(msg));
    }

    void ClientConnection::processMessage( Message&& msg )
    {
        switch(msg.header.messageClass) {
            case MessageClass::errorResponse:
            case MessageClass::successResponse:
                onRequestMessageRecv(std::move(msg));
                return;
            case MessageClass::indication:
                onIndicationMessageRecv(std::move(msg));
                return;
            default:
                return;
        }
    }

    bool ClientConnection::hasErrorAttribute( const Message& msg ) {
        for( auto& attribute : msg.attributes ) {
            if( attribute.second->type() == attrs::ERROR_CODE )
                return true;
        }
        return false;
    }

} // namespase stun
} // namespase nx
