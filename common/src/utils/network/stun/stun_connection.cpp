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
        //TODO
    }

    void StunClientConnection::onConnectionComplete( SystemError::ErrorCode ec , 
        const std::function<void(SystemError::ErrorCode)>& completion_handler ) {
        completion_handler(ec);
        if( ec ) {
            m_state = NOT_CONNECTED;
            return;
        } else {
            m_state = CONNECTED;
        }
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
                    dispatchPendingRequest();
                    return;
            }
        }
    }

    void StunClientConnection::onDataReceive( SystemError::ErrorCode ec , std::size_t bytes_transferred ) {
        if(ec) {
            m_outstandingRequest.invokeHandler(ec,nx_stun::Message());
            return;
        } else {
            if( bytes_transferred == 0 ) {
                if(m_messageParser.state() != nx_api::ParserState::done ) {
                    // We just not get the whole message , so it is an error but I don't
                    // know how to set the error code here .
                    m_outstandingRequest.invokeHandler(0,nx_stun::Message());
                    return;
                }
            } else {
                // Parsing the message
                m_messageParser.parse(m_outstandingRequest.recv_buffer,&bytes_transferred);
                shrinkBufferFor(&m_outstandingRequest.recv_buffer,bytes_transferred);
            }
            if( m_messageParser.state() == nx_api::ParserState::done ) {
                // Process the response message now
                processResponse(std::move(m_outstandingRequest.response_message));
                // Since all the pending request requires send operation so we are good
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
        if( !m_pendingRequest.empty() ) {
            m_outstandingRequest.init(std::move(m_pendingRequest.front()));
            m_pendingRequest.pop_front();
            // Setting up the serialization buffer and try to serialize it into the buffer
            serializeOutstandingRequest();
            // Setting up the parser for response message
            m_messageParser.setMessage( &(m_outstandingRequest.response_message) );
            m_messageParser.reset();
            // Sending out the buffer that we had now
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
                    dispatchPendingRequest();
                    return;
            }
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
        if( m_state == NOT_CONNECTED ) {
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
}
