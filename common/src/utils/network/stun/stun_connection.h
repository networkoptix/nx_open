/**********************************************************
* 30 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_STUN_CONNECTION_H
#define NX_STUN_CONNECTION_H

#include <functional>
#include <memory>

#include <utils/network/connection_server/base_stream_protocol_connection.h>
#include <utils/common/stoppable.h>

#include "stun_message.h"
#include "stun_message_parser.h"
#include "stun_message_serializer.h"


namespace nx_stun
{
    //!Connects to STUN server, sends requests, receives responses and indications
    /*!
        \note Methods of this class are not thread-safe
        \todo restore interrupted connection ?
    */
    class StunClientConnection
    :
        public QnStoppableAsync
    {
    public:
        //!As required by \a nx_api::BaseServerConnection
        typedef StunClientConnection ConnectionType;

        StunClientConnection(
            const SocketAddress& stunServerEndpoint,
            bool useSsl = false,
            SocketFactory::NatTraversalType natTraversalType = SocketFactory::nttAuto );
        virtual ~StunClientConnection();

        //!Implementation of QnStoppableAsync::pleaseStop
        /*!
            Cancels ongoing request (if any)
        */
        virtual void pleaseStop( std::function<void()>&& completionHandler ) override;

        //!Asynchronously openes connection to the server, specified during initialization
        /*!
            \return \a false, if could not start asynchronous operation (this can happen due to lack of resources on host machine)
            \note It is valid to call \a StunClientConnection::sendRequest when connection has not completed yet
        */
        bool openConnection( std::function<void(SystemError::ErrorCode)>&& completionHandler );
        //!Register handler to be invoked on receiving indication
        /*!
            \note \a indicationHandler will be invoked in aio thread
        */
        void registerIndicationHandler( std::function<void(nx_stun::Message&&)>&& indicationHandler );
        //!Sends message asynchronously
        /*!
            \param completionHandler Triggered after response has been received or error has occured. 
                \a Message attribute is valid only if first attribute value is \a SystemError::noError
            \return \a false, if could not start asynchronous operation (this can happen due to lack of resources on host machine)
            \note If connection to server has not been opened yet then opens one
            \note It is valid to call this method after If \a StunClientConnection::openConnection has been called and not completed yet
        */
        bool sendRequest(
            nx_stun::Message&& request,
            std::function<void(SystemError::ErrorCode, nx_stun::Message&&)>&& completionHandler );

        /*!
            \note Required by \a nx_api::BaseServerConnection
        */
        void closeConnection( StunClientConnection* );

    private:
        typedef nx_api::BaseStreamProtocolConnectionEmbeddable<
                StunClientConnection,
                nx_stun::Message,
                nx_stun::MessageParser,
                nx_stun::MessageSerializer
            > BaseConnectionType;

        std::unique_ptr<AbstractCommunicatingSocket> m_socket;
        std::unique_ptr<BaseConnectionType> m_baseConnection;
        const SocketAddress m_stunServerEndpoint;

        /*!
            \note Required by \a nx_api::BaseStreamProtocolConnection
        */
        void processMessage( nx_stun::Message&& msg );
        
        void onConnectionComplete( SystemError::ErrorCode , const std::function<void(SystemError::ErrorCode)>& );

        void onDataReceive( SystemError::ErrorCode , std::size_t );

        void onRequestSend( SystemError::ErrorCode , std::size_t );
        
        void dispatchPendingRequest();

        void processResponse( nx_stun::Message&& msg );

        void shrinkBufferFor( nx::Buffer* buf , std::size_t size ) {
            if( size >= buf->size() )
                buf->clear();
            else {
                nx::Buffer buffer(buf->constData()+size,buf->size()-size);
                buf->swap(buffer);
            }
        }

        void serializeOutstandingRequest();

        // AbstractCommunicationSocket comments says isConnected is not reliable
        // so I just add a flag to indicate whether we have connected or not
        enum {
            NOT_CONNECTED,
            CONNECTING,
            CONNECTED
        };
        // Not thread safe
        int m_state;

        // Set up a pending request list for send request operations.
        // Since 1) sendRequest needs to wait for openConnection completion
        // 2) RFC says we can have multiple outstanding request per STUN client.
        struct PendingRequest {
            nx_stun::Message request_message;
            std::function<void(SystemError::ErrorCode, nx_stun::Message&&)> completion_handler;
        };
        std::list< std::unique_ptr<PendingRequest> > m_pendingRequest;
        nx_stun::MessageParser m_messageParser;
        nx_stun::MessageSerializer m_messageSerializer;
        // Using this context to represent the current outstanding request 
        struct RequestContenxt {
            nx::Buffer recv_buffer;
            nx::Buffer send_buffer;
            nx_stun::Message response_message;
            std::unique_ptr<PendingRequest> pending_request;
            void invokeHandler( SystemError::ErrorCode ec , nx_stun::Message&& msg ) {
                pending_request->completion_handler(ec,std::move(msg));
            }
            void init( std::unique_ptr<PendingRequest>&& request ) {
                recv_buffer.clear();
                send_buffer.clear();
                response_message.clear();
                pending_request = std::move(request);
            }
        };
        RequestContenxt m_outstandingRequest;
    };
}

#endif  //NX_STUN_CONNECTION_H
