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
        public nx_api::BaseStreamProtocolConnection<
            StunClientConnection,
            StunClientConnection,
            nx_stun::Message,
            nx_stun::MessageParser,
            nx_stun::MessageSerializer>,
        public QnStoppableAsync
    {
        typedef nx_api::BaseStreamProtocolConnection<
            StunClientConnection,
            StunClientConnection,
            nx_stun::Message,
            nx_stun::MessageParser,
            nx_stun::MessageSerializer
        > BaseConnectionType;

    public:
        //!As required by \a nx_api::BaseServerConnection
        typedef StunClientConnection ConnectionType;

        StunClientConnection(
            const SocketAddress& stunServerEndpoint,
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
            \note Required by \a nx_api::BaseStreamProtocolConnection
        */
        void processMessage( nx_stun::Message&& msg );
        /*!
            \note Required by \a nx_api::BaseServerConnection
        */
        void closeConnection( StunClientConnection* );
    };
}

#endif  //NX_STUN_CONNECTION_H
