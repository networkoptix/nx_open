/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#include "stun_connection.h"

namespace nx_stun
{

    StunClientConnection::StunClientConnection(
        const SocketAddress& stunServerEndpoint,
        bool useSsl,
        SocketFactory::NatTraversalType natTraversalType ) :
        m_stunServerEndpoint( stunServerEndpoint ),
        m_state(NOT_CONNECTED)
    {
        m_socket.reset( SocketFactory::createStreamSocket( useSsl, natTraversalType ) );
        m_socket->setNonBlockingMode(true);
        m_baseConnection.reset( new BaseConnectionType( this, m_socket.get() ) );
        using namespace std::placeholders;
        m_baseConnection->setMessageHandler( [this]( nx_stun::Message&& msg ) { processMessage( std::move(msg) ); } );
    }

    void StunClientConnection::pleaseStop( std::function<void()>&& completionHandler )
    {
        // Cancel the Pending IO operations
        if( m_state != NOT_CONNECTED ) {
            m_socket->cancelAsyncIO();
            m_baseConnection->closeConnection();
            m_outstandingRequest.reset();
            if( m_state.load(std::memory_order_acquire) != CONNECTED ) {
                m_state = NOT_CONNECTED;
            }
            if( completionHandler )
                completionHandler();
        }
    }

    void StunClientConnection::enqueuePendingRequest( nx_stun::Message&& msg , std::function<void(SystemError::ErrorCode,nx_stun::Message&&)>&& func ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_ASSERT(!m_outstandingRequest);
        m_outstandingRequest.reset( new PendingRequest(std::move(msg),std::move(func)) );
    }

    void StunClientConnection::resetOutstandingRequest() {
        if( !m_outstandingRequest )
            return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outstandingRequest.reset();
    }

    void StunClientConnection::onConnectionComplete( SystemError::ErrorCode ec , 
        const std::function<void(SystemError::ErrorCode)>& completion_handler ) {
        if( completion_handler )
            completion_handler(ec);
        if( ec ) {
            m_state.store(NOT_CONNECTED,std::memory_order_release);
            return;
        } else {
            m_state.store(CONNECTED,std::memory_order_release);
        }
        SocketAddress addr = m_socket->getLocalAddress();
        // Set up the read connection. If this cannot be setup we treat it
        // as cannot finish the connection .
        if( ec == SystemError::noError && !m_baseConnection->startReadingConnection() ) {
            ec = STUN_CONN_CANNOT_READ;
        }
        bool ret = dispatchPendingRequest(ec);
        if( !ret ) {
            if( m_outstandingRequest ) {
                m_outstandingRequest->completion_handler(
                    SystemError::getLastOSErrorCode(),nx_stun::Message());
                resetOutstandingRequest();
            }
        }
    }

    void StunClientConnection::onRequestSend( SystemError::ErrorCode ec ) {
        if( ec ) {
            m_outstandingRequest->completion_handler(ec,nx_stun::Message());
            resetOutstandingRequest();
        }
    }

    // This function will _NOT_ remove pending outstanding IO event , so
    // the caller should call resetOustandingRequest per to the semantic 

    bool StunClientConnection::dispatchPendingRequest( SystemError::ErrorCode ec ) {
        // Checking if we have any pending operations
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if( !m_outstandingRequest || m_outstandingRequest->execute )
                return false;
            m_outstandingRequest->execute = true;
        }
        if( ec ) {
            m_outstandingRequest->completion_handler(ec,nx_stun::Message());
            return false;
        }
        // Dequeue the request from the pending request queue
        return m_baseConnection->sendMessage( 
            nx_stun::Message(std::move(m_outstandingRequest->request_message)),
            std::bind(
                &StunClientConnection::onRequestSend,
                this,
                std::placeholders::_1));
    }

    bool StunClientConnection::openConnection( std::function<void(SystemError::ErrorCode)>&& completionHandler )
    {
        Q_ASSERT( m_state == NOT_CONNECTED );
        m_state = CONNECTING;
        return m_socket->connectAsync(
            m_stunServerEndpoint,
            std::bind(
            &StunClientConnection::onConnectionComplete,
            this,
            std::placeholders::_1,std::move(completionHandler)));
    }

    void StunClientConnection::registerIndicationHandler( std::function<void(nx_stun::Message&&)>&& indicationHandler )
    {
        m_indicationHandler = indicationHandler;
    }

    bool StunClientConnection::sendRequest(
        nx_stun::Message&& request,
        std::function<void(SystemError::ErrorCode, nx_stun::Message&&)>&& completionHandler )
    {
        switch(m_state.load(std::memory_order_acquire)) {
        case NOT_CONNECTED:
            {
                m_state.store(CONNECTING,std::memory_order_relaxed);
                Q_ASSERT(!m_outstandingRequest);
                // The following code is not thread safe in terms of pending request queue since 
                // this is OK. Without connection, no other thread will use our API .
                m_outstandingRequest.reset(
                    new PendingRequest(std::move(request),std::move(completionHandler)));
                bool ret = openConnection(
                    std::bind(
                    &StunClientConnection::onConnectionComplete,
                    this,
                    std::placeholders::_1,
                    std::function<void(SystemError::ErrorCode)>()));
                enqueuePendingRequest(std::move(request),std::move(completionHandler));
                return ret;
            }
        case CONNECTING:
            enqueuePendingRequest( std::move(request),std::move(completionHandler) );
            return true;
        case CONNECTED:
            {
                enqueuePendingRequest( std::move(request),std::move(completionHandler) );
                if( !dispatchPendingRequest( SystemError::noError ) ) {
                    // The send error will be treated as an error happened locally so no user's 
                    // handler is invoking , otherwise potentially recursive lock can happen .
                    resetOutstandingRequest();
                    return false;
                }
                return true;
            }
        default: Q_ASSERT(0); return false;
        }
    }

    void StunClientConnection::closeConnection( BaseConnectionType* connection )
    {
        assert( connection == m_baseConnection.get() );
        m_socket->close();
    }

    void StunClientConnection::onRequestMessageRecv( nx_stun::Message& msg ) 
    {
        // Checking the message to see whether it is an valid message or not
        if( msg.header.messageClass == nx_stun::MessageClass::errorResponse ) {
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

    void StunClientConnection::onIndicationMessageRecv( nx_stun::Message& msg ) 
    {
        if( m_indicationHandler ) 
            m_indicationHandler(std::move(msg));
    }

    void StunClientConnection::processMessage( nx_stun::Message&& msg )
    {
        switch(msg.header.messageClass) {
        case nx_stun::MessageClass::errorResponse:
        case nx_stun::MessageClass::successResponse:
            onRequestMessageRecv(std::move(msg));
            return;
        case nx_stun::MessageClass::indication:
            onIndicationMessageRecv(std::move(msg));
            return;
        default: return;
        }
    }

    bool StunClientConnection::hasErrorAttribute( const nx_stun::Message& msg ) {
        for( auto& attr : msg.attributes ) {
            if(attr.second->type == nx_stun::attr::AttributeType::errorCode )
                return true;
        }
        return false;
    }
}
