/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_STREAM_PROTOCOL_CONNECTION_H
#define BASE_STREAM_PROTOCOL_CONNECTION_H

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>

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
            void processMessage( Message&& request );
        \endcode

        \note SerializerType::serialize is allowed to reallocate source buffer if needed, 
            but it is not recommended due to performance considerations!
    */
    template<
        class CustomConnectionType,
        class CustomConnectionManagerType,
        class MessageType,
        class ParserType,
        class SerializerType
    > class BaseStreamProtocolConnection
    :
        public BaseServerConnection<
            BaseStreamProtocolConnection<
                CustomConnectionType,
                CustomConnectionManagerType,
                MessageType,
                ParserType,
                SerializerType>,
            CustomConnectionManagerType>
    {
    public:
        typedef BaseStreamProtocolConnection<
            CustomConnectionType,
            CustomConnectionManagerType,
            MessageType,
            ParserType,
            SerializerType> SelfType;
        //typedef BaseStreamProtocolConnection<CustomConnectionType, CustomConnectionManagerType> SelfType;
        typedef BaseServerConnection<SelfType, CustomConnectionManagerType> BaseType;
        //!Type of messages used by this connection class. Request/response/indication is sub-class of message, so no difference at this level
        typedef MessageType message_type;

        BaseStreamProtocolConnection(
            CustomConnectionManagerType* connectionManager,
            std::unique_ptr<AbstractCommunicatingSocket> streamSocket )
        :
            BaseType( connectionManager, std::move(streamSocket) ),
            m_serializerState( SerializerState::done ),
            m_connectionFreedFlag( nullptr )
        {
            static const size_t DEFAULT_SEND_BUFFER_SIZE = 4*1024;
            m_writeBuffer.reserve( DEFAULT_SEND_BUFFER_SIZE );

            m_parser.setMessage( &m_request );
        }

        virtual ~BaseStreamProtocolConnection()
        {
            if( m_connectionFreedFlag )
                *m_connectionFreedFlag = true;
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
            // Using clear will clear the reserved buffer in QByteArray --dpeng
            m_writeBuffer.resize(0);

            if( m_serializerState != SerializerState::needMoreBufferSpace )
            {
                //message sent, triggerring completion handler
                std::function<void( SystemError::ErrorCode )> sendCompletionHandler;
                assert( !m_sendQueue.empty() );
                //completion handler is allowed to remove this connection, so moving handler to local variable
                sendCompletionHandler = std::move( m_sendQueue.front().handler );
                m_sendQueue.pop_front();

                if( sendCompletionHandler )
                {
                    bool connectionFreed = false;
                    m_connectionFreedFlag = &connectionFreed;
                    sendCompletionHandler( SystemError::noError );
                    if( connectionFreed )
                        return; //connection has been removed by handler
                    m_connectionFreedFlag = nullptr;
                }

                processAnotherSendTaskIfAny();
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
            if( !BaseType::sendBufAsync( m_writeBuffer ) )
                reportErrorAndCloseConnection( SystemError::getLastOSErrorCode() );

            //on completion readyToSendData will be called
        }

        //TODO #ak add message body streaming

        /*!
            Initiates asynchoronous message send
        */
        template<class Handler>
        bool sendMessage( MessageType msg, Handler&& handler )
        {
            auto newTask = std::make_shared<SendTask>(
                std::move( msg ),
                std::forward<Handler>( handler ) ); //TODO #ak get rid of this when generic lambdas are available
            return this->socket()->dispatch(
                [this, newTask]()
                {
                    m_sendQueue.push_back( std::move( *newTask ) );
                    if( m_sendQueue.size() > 1 )
                        return; //already processing another message

                    processAnotherSendTaskIfAny();
                } );
        }

        bool sendMessage( MessageType msg ) // template cannot be resolved by default value
        {
            return sendMessage( std::move(msg),
                                std::function< void( SystemError::ErrorCode ) >() );
        }

        template<class BufferType, class Handler>
        bool sendData( BufferType&& data, Handler&& handler )
        {
            auto newTask = std::make_shared<SendTask>(
                std::forward<BufferType>( data ),
                std::forward<Handler>( handler ) );
            return this->socket()->dispatch(
                [this, newTask]()
                {
                    m_sendQueue.push_back( std::move( *newTask ) );
                    if( m_sendQueue.size() > 1 )
                        return; //already processing another message

                    processAnotherSendTaskIfAny();
                } );
        }

    private:
        struct SendTask
        {
        public:
            SendTask()
            :
                asyncSendIssued( false )
            {
            }

            template<typename HandlerRefType>
                SendTask(
                    MessageType&& _msg,
                    HandlerRefType&& _handler )
            :
                msg( std::move(_msg) ),
                handler( std::forward<HandlerRefType>(_handler) ),
                asyncSendIssued( false )
            {
            }

            template<typename HandlerRefType>
                SendTask(
                    nx::Buffer&& _buf,
                    HandlerRefType&& _handler )
            :
                buf( std::move(_buf) ),
                handler( std::forward<HandlerRefType>(_handler) ),
                asyncSendIssued( false )
            {
            }

            SendTask( SendTask&& right )
            :
                msg( std::move( right.msg ) ),
                buf( std::move( right.buf ) ),
                handler( std::move( right.handler ) ),
                asyncSendIssued( right.asyncSendIssued )
            {
            }

            MessageType msg;
            boost::optional<nx::Buffer> buf;
            std::function<void( SystemError::ErrorCode )> handler;
            bool asyncSendIssued;

        private:
            SendTask( const SendTask& );
            SendTask& operator=( const SendTask& );
        };

        MessageType m_request;
        ParserType m_parser;
        SerializerType m_serializer;
        SerializerState::Type m_serializerState;
        nx::Buffer m_writeBuffer;
        std::function<void(SystemError::ErrorCode)> m_sendCompletionHandler;
        std::deque<SendTask> m_sendQueue;
        bool* m_connectionFreedFlag;

        void sendMessageInternal( MessageType&& msg )
        {
            //serializing message
            m_serializer.setMessage( std::move(msg) );
            m_serializerState = SerializerState::needMoreBufferSpace;
            readyToSendData();
        }

        void processAnotherSendTaskIfAny()
        {
            //sending next message in queue (if any)
            if( m_sendQueue.empty() || m_sendQueue.front().asyncSendIssued )
                return;
            auto& task = m_sendQueue.front();
            task.asyncSendIssued = true;
            if( task.buf )
            {
                assert( m_writeBuffer.isEmpty() );
                m_writeBuffer = std::move( task.buf.get() );
                m_serializerState = SerializerState::done;
                if( !BaseType::sendBufAsync( m_writeBuffer ) )
                    reportErrorAndCloseConnection( SystemError::getLastOSErrorCode() );
            }
            else //if( task.msg )
            {
                sendMessageInternal( std::move( task.msg ) );
            }
        }

        void reportErrorAndCloseConnection( SystemError::ErrorCode errorCode )
        {
            bool connectionFreed = false;
            m_connectionFreedFlag = &connectionFreed;
            assert( !m_sendQueue.empty() );
            auto handler = std::move( m_sendQueue.front().handler );
            handler( errorCode );
            if( connectionFreed )
                return; //connection has been removed by handler
            m_connectionFreedFlag = nullptr;
            this->connectionManager()->closeConnection(
                static_cast<typename CustomConnectionManagerType::ConnectionType*>(this) );
        }
    };

    /*!
        Inherits \a BaseStreamProtocolConnection and delegates \a processMessage to functor set with \a BaseStreamProtocolConnectionEmbeddable::setMessageHandler
        \todo Remove this class or get rid of virtual call (in function)
    */
    template<
        class CustomConnectionManagerType,
        class MessageType,
        class ParserType,
        class SerializerType
    > class BaseStreamProtocolConnectionEmbeddable
    :
        public BaseStreamProtocolConnection<
            BaseStreamProtocolConnectionEmbeddable<
                CustomConnectionManagerType,
                MessageType,
                ParserType,
                SerializerType>,
            CustomConnectionManagerType,
            MessageType,
            ParserType,
            SerializerType>
    {
        typedef BaseStreamProtocolConnectionEmbeddable<
            CustomConnectionManagerType,
            MessageType,
            ParserType,
            SerializerType
        > self_type;
        typedef BaseStreamProtocolConnection<
            self_type,
            CustomConnectionManagerType,
            MessageType,
            ParserType,
            SerializerType
        > base_type;

    public:
        BaseStreamProtocolConnectionEmbeddable(
            CustomConnectionManagerType* connectionManager,
            std::unique_ptr<AbstractCommunicatingSocket> streamSocket )
        :
            base_type(
                connectionManager,
                std::move(streamSocket) )
        {
        }

        template<class T>
        void setMessageHandler( T&& handler )
        {
            m_handler = std::forward<T>(handler);
        }

        void processMessage( MessageType&& msg )
        {
            if( m_handler )
                m_handler( std::move(msg) );
        }

    private:
        std::function<void(MessageType&&)> m_handler;
    };
}

#endif   //BASE_STREAM_PROTOCOL_CONNECTION_H
