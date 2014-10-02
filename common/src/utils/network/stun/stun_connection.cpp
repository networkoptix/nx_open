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
        SocketFactory::NatTraversalType natTraversalType )
    :
    //    BaseConnectionType( this, nullptr )
        m_stunServerEndpoint( stunServerEndpoint ),
        m_state(NOT_CONNECTED)
    {
        //TODO creating connection

        m_socket.reset( SocketFactory::createStreamSocket( useSsl, natTraversalType ) );
        m_baseConnection.reset( new BaseConnectionType( this, m_socket.get() ) );
        using namespace std::placeholders;
        m_baseConnection->setMessageHandler( [this]( nx_stun::Message&& msg ) { processMessage( std::move(msg) ); } );
    }

    StunClientConnection::~StunClientConnection()
    {
        //TODO
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
            completionHandler();
        }
    }

    bool StunClientConnection::enqueuePendingRequest( nx_stun::Message&& msg , std::function<void(SystemError::ErrorCode,nx_stun::Message&&)>&& func ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if( m_outstandingRequest )
            return false;
        m_outstandingRequest.reset( new PendingRequest(std::move(msg),std::move(func)) );
        return true;
    }

    void StunClientConnection::resetOutstandingRequest() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outstandingRequest.reset();
    }

    void StunClientConnection::onConnectionComplete( SystemError::ErrorCode ec , 
        const std::function<void(SystemError::ErrorCode)>& completion_handler ) {
        // Invoke user handler, this handler could be NULL since if this function is issued
        // internally inside of the sendRequest, we will not give a valid completion handler
        if( completion_handler )
            completion_handler(ec);
        dispatchPendingRequest();
        if( ec ) {
            m_state.store(NOT_CONNECTED,std::memory_order_release);
            return;
        } else {
            m_state.store(CONNECTED,std::memory_order_release);
        }
    }

    void StunClientConnection::onRequestSend( SystemError::ErrorCode ec ) {
        if( ec ) {
            m_outstandingRequest->completion_handler(ec,nx_stun::Message());
            resetOutstandingRequest();
        }
        // Start reading response from the message queue
        if(!m_baseConnection->startReadingConnection()) {
            m_outstandingRequest->completion_handler(
                SystemError::getLastOSErrorCode(),nx_stun::Message());
            resetOutstandingRequest();
        }
    }

    void StunClientConnection::dispatchPendingRequest() {
        // Checking if we have any pending operations
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if( !m_outstandingRequest || m_outstandingRequest->execute )
                return;
            // The flag set up must be inside of the lock
            m_outstandingRequest->execute = true;
        }
        // Dequeue the request from the pending request queue
        bool ret = m_baseConnection->sendMessage( 
            nx_stun::Message(m_outstandingRequest->request_message),
            std::bind(
            &StunClientConnection::onRequestSend,
            this,
            std::placeholders::_1));
        Q_ASSERT(ret);
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
        //TODO
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
                if(ret) {
                    ret = enqueuePendingRequest(std::move(request),std::move(completionHandler));
                    Q_ASSERT(ret);
                } else {
                    return false;
                }
            }
        case CONNECTING:
        case CONNECTED:
            {
                if( enqueuePendingRequest(std::move(request),std::move(completionHandler)) ) {
                    dispatchPendingRequest();
                    return true;
                }
                return false;
            }
        default: Q_ASSERT(0); return false;
        }
    }

    void StunClientConnection::closeConnection( StunClientConnection* connection )
    {
        connection->m_socket->close();
    }

    void StunClientConnection::processMessage( nx_stun::Message&& msg )
    {
        // Checking the message to see whether it is an valid message or not
        if( msg.header.messageClass == nx_stun::MessageClass::errorResponse ) {
            SystemError::setLastErrorCode(STUN_REQUEST_FAIL);
            m_outstandingRequest->completion_handler(STUN_REQUEST_FAIL,std::move(msg));
        } else {
            // If we detect an error attributes here, we treat the package is broken or not
            // STUN protocol compatible, this should not happen in our server configuration
            if( hasErrorAttribute(msg) ) {
                SystemError::setLastErrorCode(STUN_REPLY_PACKAGE_BROKEN);
                m_outstandingRequest->completion_handler(STUN_REPLY_PACKAGE_BROKEN,std::move(msg));
            } else {
                m_outstandingRequest->completion_handler(0,std::move(msg));
            }
        }
        // This message chain processing is finished here
        resetOutstandingRequest();
    }

    bool StunClientConnection::hasErrorAttribute( const nx_stun::Message& msg ) {
        for( auto& attr : msg.attributes ) {
            if(attr.second->type == nx_stun::attr::AttributeType::errorCode )
                return true;
        }
        return false;
    }
}
