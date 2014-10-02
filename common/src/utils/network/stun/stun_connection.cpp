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
            m_pendingRequestQueue.clear();
            if( m_state.load(std::memory_order_acquire) != CONNECTED ) {
                m_state = NOT_CONNECTED;
            }
        }
    }

    bool StunClientConnection::dequeuePendingRequest() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if( !m_outstandingRequest.isEmpty() )
            return false;
        m_outstandingRequest.init(
            std::move(m_pendingRequestQueue.front()));
        m_pendingRequestQueue.pop_front();
        return true;
    }

    void StunClientConnection::enqueuePendingRequest( nx_stun::Message&& msg , std::function<void(SystemError::ErrorCode,nx_stun::Message&&)>&& func ) {
        std::list<std::unique_ptr<PendingRequest>> element;
        element.push_back(std::unique_ptr<PendingRequest>( new PendingRequest(std::move(msg),std::move(func))));
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingRequestQueue.splice(m_pendingRequestQueue.end(),std::move(element));
        }
    }

    void StunClientConnection::onConnectionComplete( SystemError::ErrorCode ec , 
        const std::function<void(SystemError::ErrorCode)>& completion_handler ) {
        // The state needs to be visible for thread that calls sendRequest otherwise
        // there may be request that permanently not be issued.
        if( ec ) {
            m_state.store(NOT_CONNECTED,std::memory_order_release);
            return;
        } else {
            m_state.store(CONNECTED,std::memory_order_release);
        }
        // Invoke user handler, this handler could be NULL since if this function is issued
        // internally inside of the sendRequest, we will not give a valid completion handler
        if( completion_handler )
            completion_handler(ec);
        dispatchPendingRequest();
    }

    void StunClientConnection::onRequestSend( SystemError::ErrorCode ec , std::size_t bytes_transferred ) {
        if( ec ) {
            m_outstandingRequest.invokeHandler(ec,nx_stun::Message());
            dispatchPendingRequest();
        } else {
            if(!m_socket->readSomeAsync(
                &(m_outstandingRequest.recv_buffer),
                std::bind(
                &StunClientConnection::onDataReceive,
                this,
                std::placeholders::_1,
                std::placeholders::_2))) {
                    m_outstandingRequest.invokeHandler(
                        SystemError::getLastOSErrorCode(),nx_stun::Message());
                    resetOutstandingRequest();
                    dispatchPendingRequest();
                    return;
            }
        }
    }

    void StunClientConnection::onDataReceive( SystemError::ErrorCode ec , std::size_t bytes_transferred ) {
        if(ec) {
            m_outstandingRequest.invokeHandler(ec,nx_stun::Message());
        } else {
            if( bytes_transferred == 0 ) {
                // We must have already seen the whole message or we haven't seen the whole message
                // in either case we are safe to release the outstanding request now.
                m_outstandingRequest.invokeHandler(STUN_REPLY_PACKAGE_BROKEN,nx_stun::Message());
                resetOutstandingRequest();
                dispatchPendingRequest();
            } else {
                // Parsing the message
                int state = m_messageParser.parse(m_outstandingRequest.recv_buffer,&bytes_transferred);
                if( state == nx_api::ParserState::inProgress ) {
                    shrinkBufferFor(&m_outstandingRequest.recv_buffer,bytes_transferred);
                    if(m_socket->readSomeAsync(
                        &(m_outstandingRequest.recv_buffer),
                        std::bind(
                        &StunClientConnection::onDataReceive,
                        this,
                        std::placeholders::_1,
                        std::placeholders::_2))) {
                            return;
                    } else {
                        m_outstandingRequest.invokeHandler(
                            SystemError::getLastOSErrorCode(),nx_stun::Message());
                    }
                } else if( state == nx_api::ParserState::done ) {
                    // Process the response and invoke user's callback function
                    processResponse(std::move(m_outstandingRequest.response_message));
                } else {
                    m_outstandingRequest.invokeHandler(STUN_REPLY_PACKAGE_BROKEN,nx_stun::Message());
                }
                resetOutstandingRequest();
                dispatchPendingRequest();
            }
        }
    }

    void StunClientConnection::serializeOutstandingRequest() {
        static const std::size_t kInitialSerializationBufferSize = 4096;
        static const std::size_t kIncFactor = 2;
        int state;
        size_t capacity = kInitialSerializationBufferSize;
        do {
            m_outstandingRequest.send_buffer.reserve(capacity);
            size_t bytes_write;
            state = m_messageSerializer.serialize(
                &(m_outstandingRequest.send_buffer),&bytes_write);
            capacity *= kIncFactor;
        } while(state == nx_api::SerializerState::needMoreBufferSpace);
    }

    void StunClientConnection::dispatchPendingRequest() {
        // Dequeue the request from the pending request queue
        if(!dequeuePendingRequest())
            return;
        // Setting up the serialization buffer and try to serialize it into the buffer
        serializeOutstandingRequest();
        // Setting up the parser for response message
        m_messageParser.setMessage( &(m_outstandingRequest.response_message) );
        m_messageParser.reset();
        // Sending out the buffer that we have now
        if(!m_socket->sendAsync(
            m_outstandingRequest.send_buffer,
            std::bind(
            &StunClientConnection::onRequestSend,
            this,
            std::placeholders::_1,
            std::placeholders::_2))) {
                // Invoke handler for error
                m_outstandingRequest.invokeHandler(
                    SystemError::getLastOSErrorCode(),nx_stun::Message());
                // Processing next pending request again
                resetOutstandingRequest();
                dispatchPendingRequest();
                return;
        }
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
                // The following code is not thread safe in terms of pending request queue since 
                // this is OK. Without connection, no other thread will use our API .
                m_pendingRequestQueue.push_back(
                    std::unique_ptr<PendingRequest>(
                    new PendingRequest(std::move(request),std::move(completionHandler))));
                return openConnection(
                    std::bind(
                    &StunClientConnection::onConnectionComplete,
                    this,
                    std::placeholders::_1,
                    std::function<void(SystemError::ErrorCode)>()));
            }
        case CONNECTING:
            {
                m_pendingRequestQueue.push_back(
                    std::unique_ptr<PendingRequest>(
                    new PendingRequest(std::move(request),std::move(completionHandler))));
                return true;
            }
        case CONNECTED:
            {
                m_pendingRequestQueue.push_back(
                    std::unique_ptr<PendingRequest>(
                    new PendingRequest(std::move(request),std::move(completionHandler))));
                dispatchPendingRequest();
                return true;
            }
        }
        return false;
    }

    void StunClientConnection::closeConnection( StunClientConnection* connection )
    {
        //TODO
    }

    void StunClientConnection::processMessage( nx_stun::Message&& msg )
    {
        //TODO
    }

    void StunClientConnection::processResponse( nx_stun::Message&& msg ) {
        // Checking the message to see whether it is an valid message or not
        if( msg.header.messageClass == nx_stun::MessageClass::errorResponse ) {
            SystemError::setLastErrorCode(STUN_REQUEST_FAIL);
            m_outstandingRequest.invokeHandler(STUN_REQUEST_FAIL,std::move(msg));
        } else {
            // If we detect an error attributes here, we treat the package is broken or not
            // STUN protocol compatible, this should not happen in our server configuration
            if( hasErrorAttribute(msg) ) {
                SystemError::setLastErrorCode(STUN_REPLY_PACKAGE_BROKEN);
                m_outstandingRequest.invokeHandler(STUN_REPLY_PACKAGE_BROKEN,std::move(msg));
            } else {
                m_outstandingRequest.invokeHandler(0,std::move(msg));
            }
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
