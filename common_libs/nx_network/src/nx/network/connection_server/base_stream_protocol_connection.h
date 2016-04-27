/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_STREAM_PROTOCOL_CONNECTION_H
#define BASE_STREAM_PROTOCOL_CONNECTION_H

#include <deque>
#include <functional>

#include <boost/optional.hpp>

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
        \note It is allowed to free instance within event handler
    */
    template<
        class CustomConnectionType,
        class MessageType,
        class ParserType,
        class SerializerType
    > class BaseStreamProtocolConnection
    :
		public BaseServerConnection<CustomConnectionType>
    {
    public:
        typedef BaseStreamProtocolConnection<
            CustomConnectionType,
            MessageType,
            ParserType,
            SerializerType> SelfType;
        typedef BaseServerConnection<CustomConnectionType> BaseType;
        //!Type of messages used by this connection class. Request/response/indication is sub-class of message, so no difference at this level
        typedef MessageType message_type;

        BaseStreamProtocolConnection(
            StreamConnectionHolder<CustomConnectionType>* connectionManager,
            std::unique_ptr<AbstractCommunicatingSocket> streamSocket )
        :
            BaseType( connectionManager, std::move(streamSocket) ),
            m_serializerState( SerializerState::done )
        {
            static const size_t DEFAULT_SEND_BUFFER_SIZE = 4*1024;
            m_writeBuffer.reserve( DEFAULT_SEND_BUFFER_SIZE );

            m_parser.setMessage( &m_request );
        }

        virtual ~BaseStreamProtocolConnection()
        {
        }

        void bytesReceived( const nx::Buffer& buf )
        {
            for( size_t pos = 0; pos < (size_t)buf.size(); )
            {
                //parsing message
                size_t bytesProcessed = 0;
                switch( m_parser.parse( pos > 0 ? buf.mid((int)pos) : buf, &bytesProcessed ) )
                {
                    case ParserState::init:
                    case ParserState::inProgress:
                        NX_ASSERT( bytesProcessed == (size_t)buf.size() );
                        return;

                    case ParserState::done:
                    {
                        //processing request
                        //NOTE interleaving is not supported yet

                        {
                            nx::utils::ObjectDestructionFlag::Watcher watcher(
                                &m_connectionFreedFlag);
                            static_cast<CustomConnectionType*>(this)->processMessage(
                                std::move(m_request));
                            if (watcher.objectDestroyed())
                                return; //connection has been removed by handler
                        }

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
                NX_ASSERT( !m_sendQueue.empty() );
                //completion handler is allowed to remove this connection, so moving handler to local variable
                sendCompletionHandler = std::move( m_sendQueue.front().handler );
                m_serializer.setMessage( nullptr );
                m_sendQueue.pop_front();

                if( sendCompletionHandler )
                {
                    nx::utils::ObjectDestructionFlag::Watcher watcher(
                        &m_connectionFreedFlag);
                    sendCompletionHandler( SystemError::noError );
                    if (watcher.objectDestroyed())
                        return; //connection has been removed by handler
                }

                processAnotherSendTaskIfAny();
                return;
            }
            size_t bytesWritten = 0;
            m_serializerState = m_serializer.serialize( &m_writeBuffer, &bytesWritten );
            if( (m_serializerState == SerializerState::needMoreBufferSpace) && (bytesWritten == 0) )
            {
                //TODO #ak increase buffer
                NX_ASSERT( false );
            }
            //assuming that all bytes will be written or none
            BaseType::sendBufAsync(m_writeBuffer);

            //on completion readyToSendData will be called
        }

        //TODO #ak add message body streaming

        /*!
            Initiates asynchoronous message send
        */
        void sendMessage(
            MessageType msg,
            std::function<void(SystemError::ErrorCode)> handler)
        {
            auto newTask = std::make_shared<SendTask>(
                std::move(msg),
                std::move(handler)); //TODO #ak get rid of shared_ptr here when generic lambdas are available
            addNewTaskToQueue(std::move(newTask));
        }

        void sendMessage(MessageType msg) // template cannot be resolved by default value
        {
            sendMessage(
                std::move(msg),
                std::function< void(SystemError::ErrorCode) >());
        }

        void sendData(
            nx::Buffer data,
            std::function<void(SystemError::ErrorCode)> handler)
        {
            auto newTask = std::make_shared<SendTask>(
                std::move(data),
                std::move(handler));
            addNewTaskToQueue(std::move(newTask));
        }

    private:
        struct SendTask
        {
        public:
            SendTask()
            :
                asyncSendIssued(false)
            {
            }

            SendTask(
                MessageType _msg,
                std::function<void(SystemError::ErrorCode)> _handler)
            :
                msg(std::move(_msg)),
                handler(std::move(_handler)),
                asyncSendIssued(false)
            {
            }

            SendTask(
                nx::Buffer _buf,
                std::function<void(SystemError::ErrorCode)> _handler)
            :
                buf(std::move(_buf)),
                handler(std::move(_handler)),
                asyncSendIssued(false)
            {
            }

            SendTask(SendTask&& right)
            :
                msg(std::move(right.msg)),
                buf(std::move(right.buf)),
                handler(std::move(right.handler)),
                asyncSendIssued(right.asyncSendIssued)
            {
            }

            boost::optional<MessageType> msg;
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
        nx::utils::ObjectDestructionFlag m_connectionFreedFlag;

        void sendMessageInternal( const MessageType& msg )
        {
            //serializing message
            m_serializer.setMessage( &msg );
            m_serializerState = SerializerState::needMoreBufferSpace;
            readyToSendData();
        }

        void addNewTaskToQueue( std::shared_ptr<SendTask> newTask )
        {
            this->socket()->dispatch(
                [this, newTask]()
                {
                    m_sendQueue.push_back( std::move( *newTask ) );
                    if( m_sendQueue.size() > 1 )
                        return; //already processing another message

                    processAnotherSendTaskIfAny();
                } );
        }

        void processAnotherSendTaskIfAny()
        {
            //sending next message in queue (if any)
            if( m_sendQueue.empty() || m_sendQueue.front().asyncSendIssued )
                return;
            auto& task = m_sendQueue.front();
            task.asyncSendIssued = true;
            if( task.msg )
            {
                sendMessageInternal( task.msg.get() );
            }
            else if( task.buf )
            {
                NX_ASSERT( m_writeBuffer.isEmpty() );
                m_writeBuffer = std::move( task.buf.get() );
                m_serializerState = SerializerState::done;
                BaseType::sendBufAsync( m_writeBuffer );
            }
        }

        void reportErrorAndCloseConnection( SystemError::ErrorCode errorCode )
        {
            NX_ASSERT( !m_sendQueue.empty() );
            auto handler = std::move( m_sendQueue.front().handler );
            {
                nx::utils::ObjectDestructionFlag::Watcher watcher(
                    &m_connectionFreedFlag);
                handler(errorCode);
                if (watcher.objectDestroyed())
                    return; //connection has been removed by handler
            }
            this->connectionManager()->closeConnection(
                errorCode,
                static_cast<CustomConnectionType*>(this) );
        }
    };

    /*!
        Inherits \a BaseStreamProtocolConnection and delegates \a processMessage to functor set with \a BaseStreamProtocolConnectionEmbeddable::setMessageHandler
        \todo Remove this class or get rid of virtual call (in function)
    */
    template<
        class MessageType,
        class ParserType,
        class SerializerType
    > class BaseStreamProtocolConnectionEmbeddable
    :
        public BaseStreamProtocolConnection<
            BaseStreamProtocolConnectionEmbeddable<
                MessageType,
                ParserType,
                SerializerType>,
            MessageType,
            ParserType,
            SerializerType>
    {
        typedef BaseStreamProtocolConnectionEmbeddable<
            MessageType,
            ParserType,
            SerializerType
        > self_type;
        typedef BaseStreamProtocolConnection<
            self_type,
            MessageType,
            ParserType,
            SerializerType
        > base_type;

    public:
        BaseStreamProtocolConnectionEmbeddable(
            StreamConnectionHolder<self_type>* connectionManager,
            std::unique_ptr<AbstractCommunicatingSocket> streamSocket )
        :
            base_type(
                connectionManager,
                std::move(streamSocket) )
        {
        }

        /** \a handler will receive all incoming messages.
            \note It is required to call \a BaseStreamProtocolConnection::startReadingConnection
                to start receiving messages
        */
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
