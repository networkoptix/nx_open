/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_STUN_CONNECTION_H
#define BASE_STUN_CONNECTION_H

#include <atomic>

#include <boost/optional.hpp>

#include "stun_message.h"
#include "stun_message_parser.h"
#include "stun_message_parse_handler.h"
#include "stun_message_serializer.h"
#include "../base_server_connection.h"


namespace nx_stun
{
    //!Connection of STUN protocol
    /*!
        It is not tied to underlying transport (tcp, udp, etc...)

        \a CustomStunConnectionType MUST provide following method:
        \code {*.cpp}
            void processMessage( const Message& request, boost::optional<Message>* const response );
        \endcode

        \todo This class contains no STUN-specific logic, so it MUST be protocol-independent (it will be used for HTTP later)
    */
    template<
        class CustomStunConnectionType,
        class CustomSocketServerType
    > class BaseStunServerConnection
    :
        public BaseServerConnection<BaseStunServerConnection<CustomStunConnectionType, CustomSocketServerType>, CustomSocketServerType>
    {
    public:
        typedef BaseStunServerConnection<CustomStunConnectionType, CustomSocketServerType> SelfType;
        typedef BaseServerConnection<SelfType, CustomSocketServerType> BaseType;

        BaseStunServerConnection(
            CustomSocketServerType* streamServer,
            AbstractStreamSocket* streamSocket )
        :
            BaseType( streamServer, streamSocket ),
            m_parseHandler( &m_request ),
            m_parser( &m_parseHandler ),
            m_serializerState( SerializerState::done ),
            m_isSendingMessage( 0 )
        {
            static const size_t DEFAULT_SEND_BUFFER_SIZE = 4*1024;
            m_writeBuffer.reserve( DEFAULT_SEND_BUFFER_SIZE );
        }

        void bytesReceived( const nx::Buffer& buf )
        {
            for( size_t pos = 0; pos < buf.size(); )
            {
                //parsing message
                size_t bytesProcessed = 0;
                switch( m_parser.parse( pos > 0 ? buf.substr(pos) : buf, &bytesProcessed ) )
                {
                    case ParserState::init:
                    case ParserState::inProgress:
                        assert( bytesProcessed == buf.size() );
                        return;

                    case ParserState::done:
                    {
                        //processing request
                        //NOTE interleaving is not supported yet
                        const bool canProcessResponse = m_isSendingMessage == 0;
                        static_cast<CustomStunConnectionType*>(this)->processMessage( m_request, canProcessResponse ? &m_response : nullptr );
                        m_parser.reset();
                        m_request.clear();
                        if( canProcessResponse && m_response )
                            sendResponseMessage();
                        pos += bytesProcessed;
                        break;
                    }

                    case ParserState::failed:
                        //TODO/IMPL: #ak ignore all following data and close connection?
                        return;
                }
            }
        }

        void readyToSendData()
        {
            assert( m_writeBuffer.empty() );
            m_writeBuffer.clear();

            if( m_serializerState != SerializerState::needMoreBufferSpace )
            {
                //can accept another outgoing message
                m_isSendingMessage = 0;
                return;
            }
            size_t bytesWritten = 0;
            m_serializerState = m_serializer.serialize( &m_writeBuffer, &bytesWritten );
            if( (m_serializerState == SerializerState::needMoreBufferSpace) && (bytesWritten == 0) )
            {
                //TODO/IMPL #ak increase buffer
                assert( false );
            }
            //assuming that all bytes will be written or none
            sendBufAsync( m_writeBuffer );
        }

    private:
        Message m_request;
        boost::optional<Message> m_response;
        MessageParseHandler m_parseHandler;
        MessageParser<MessageParseHandler> m_parser;
        MessageSerializer m_serializer;
        SerializerState m_serializerState;
        nx::Buffer m_writeBuffer;
        std::atomic<int> m_isSendingMessage;

        void sendResponseMessage()
        {
            //serializing message
            m_serializer.setMessage( m_response.get() );
            m_serializerState = SerializerState::needMoreBufferSpace;
            m_isSendingMessage = 1;
            readyToSendData();
        }
    };
}

#endif  //BASE_STUN_CONNECTION_H
