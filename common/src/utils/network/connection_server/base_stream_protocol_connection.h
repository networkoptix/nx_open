/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_STREAM_PROTOCOL_CONNECTION_H
#define BASE_STREAM_PROTOCOL_CONNECTION_H

#include <atomic>
#include <functional>

#include "base_protocol_message_types.h"
#include "base_server_connection.h"


namespace nx_api
{
    //!Connection of stream-orientied protocol of type request/respose
    /*!
        It is not tied to underlying transport (tcp, udp, etc...)

        \a CustomConnectionType MUST provide following methods:
        \code {*.cpp}
            //!Called for every received message 
            void processMessage( const Message& request );
            //!Called after sending message (e.g. response)
            void messageSent();
        \endcode
    */
    template<
        class CustomConnectionType,
        class CustomSocketServerType,
        class MessageType,
        class ParserType,
        class SerializerType
    > class BaseStreamProtocolConnection
    :
        public BaseServerConnection<
            BaseStreamProtocolConnection<
                CustomConnectionType,
                CustomSocketServerType,
                MessageType,
                ParserType,
                SerializerType>,
            CustomSocketServerType>
    {
    public:
        typedef BaseStreamProtocolConnection<
            CustomConnectionType,
            CustomSocketServerType,
            MessageType,
            ParserType,
            SerializerType> SelfType;
        //typedef BaseStreamProtocolConnection<CustomConnectionType, CustomSocketServerType> SelfType;
        typedef BaseServerConnection<SelfType, CustomSocketServerType> BaseType;
        //!Type of messages used by this connection class. Request/response/indication is sub-class of message, so no difference at this level
        typedef MessageType message_type;

        BaseStreamProtocolConnection(
            CustomSocketServerType* streamServer,
            AbstractCommunicatingSocket* streamSocket )
        :
            BaseType( streamServer, streamSocket ),
            m_serializerState( SerializerState::done ),
            m_isSendingMessage( 0 )
        {
            static const size_t DEFAULT_SEND_BUFFER_SIZE = 4*1024;
            m_writeBuffer.reserve( DEFAULT_SEND_BUFFER_SIZE );

            m_parser.setMessage( &m_request );
        }

        void bytesReceived( const nx::Buffer& buf )
        {
            for( size_t pos = 0; pos < buf.size(); )
            {
                //parsing message
                size_t bytesProcessed = 0;
                switch( m_parser.parse( pos > 0 ? buf.mid((int)pos) : buf, &bytesProcessed ) )
                {
                    case ParserState::init:
                    case ParserState::inProgress:
                        assert( bytesProcessed == buf.size() );
                        return;

                    case ParserState::done:
                    {
                        //processing request
                        //NOTE interleaving is not supported yet
                        static_cast<CustomConnectionType*>(this)->processMessage( std::move(m_request) );
                        m_parser.reset();
                        m_request.clear();
                        m_parser.setMessage( &m_request );
                        pos += bytesProcessed;
                        break;
                    }

                    case ParserState::failed:
                        //TODO : #ak ignore all following data and close connection?
                        return;
                }
            }
        }

        void readyToSendData()
        {
            m_writeBuffer.clear();

            if( m_serializerState != SerializerState::needMoreBufferSpace )
            {
                //can accept another outgoing message
                m_isSendingMessage = 0;
                if( m_sendCompletionHandler )
                {
                    //completion handler is allowed to remove this connection
                    auto sendCompletionHandlerBak = std::move( m_sendCompletionHandler );
                    sendCompletionHandlerBak();
                }
                return;
            }
            size_t bytesWritten = 0;
            m_serializerState = m_serializer.serialize( &m_writeBuffer, &bytesWritten );
            if( (m_serializerState == SerializerState::needMoreBufferSpace) && (bytesWritten == 0) )
            {
                //TODO #ak increase buffer
                assert( false );
            }
            //assuming that all bytes will be written or none
            sendBufAsync( m_writeBuffer );
        }

        /*!
            Initiates asynchoronous message send
        */
        template<class Handler>
        bool sendMessage( MessageType&& msg, Handler handler = std::function<void()>() )
        {
            if( m_isSendingMessage > 0 )
                return false;   //TODO: #ak interleaving is not supported yet

            m_sendCompletionHandler = std::move(handler);
            sendResponseMessage( std::move(msg) );
            return true;
        }

    private:
        MessageType m_request;
        ParserType m_parser;
        SerializerType m_serializer;
        SerializerState::Type m_serializerState;
        nx::Buffer m_writeBuffer;
        std::atomic<int> m_isSendingMessage;
        std::function<void()> m_sendCompletionHandler;

        void sendResponseMessage( MessageType&& msg )
        {
            //serializing message
            m_serializer.setMessage( std::move(msg) );
            m_serializerState = SerializerState::needMoreBufferSpace;
            m_isSendingMessage = 1;
            readyToSendData();
        }
    };
}

#endif   //BASE_STREAM_PROTOCOL_CONNECTION_H
