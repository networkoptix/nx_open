/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_CONNECTION_H
#define NX_MEDIATOR_CONNECTION_H

#include <functional>

#include <utils/common/stoppable.h>
#include <utils/network/socket_common.h>

#include "cc_common.h"


namespace nx_cc
{
    //TODO #ak backward tcp connection support

    //!Server receives this indication when some other peer is trying to connect via mediator
    class IncomingConnectionIndication
    {
    public:
        //!Address of udp hole of initiating peer. Server should try establish rendezvous UDT connection with this address
        SocketAddress remotePeerUDPHoleAddress;
    };

    //!Peer sends this request to initiate connection to the server listening on mediator
    class ConnectionRequest
    {
    public:
        //!Address of host we want to connect to
        HostAddress targetHost;
    };

    class ConnectionResponse
    {
    public:
        //!Address of udp hole of listening server. Client can try to establish rendezvous UDT connection with this address
        SocketAddress serverUDPHoleAddress;
    };

    //!Connection to the mediator
    class MediatorConnection
    :
        public QnStoppableAsync
    {
    public:
        MediatorConnection();
        virtual ~MediatorConnection();

        //!Implementation of QnStoppableAsync::pleaseStop
        virtual void pleaseStop( std::function<void()>&& completionHandler ) override;

        /***************************
        *     Server operations    *
        ****************************/
        //!Bind to address \a addrToListen on mediator
        /*!
            \param addrToListen This address MUST be unique. No \a SO_REUSEADDR analogue is present
            \return \a true if async bind operation started successfully
        */
        bool bind( const HostAddress& addrToListen, std::function<void(ErrorDescription)>&& completionHandler );
        //!Register "incoming connection" indication handler
        void registerIndicationHandler( std::function<void(IncomingConnectionIndication)>&& indicationHandler );

        /***************************
        *     Client operations    *
        ****************************/

        //!Initiate asynchronous connection to mediator on address \a mediatorAddress
        /*!
            Connection to the mediator is established with UDT protocol
            \return \a true if async request started successfully
        */
        bool connectAsync( const SocketAddress& mediatorAddress, std::function<void(ErrorDescription)>&& completionHandler );
        //!Request connection to server \a request.targetHost listening on mediator
        /*!
            \param completionHandler \a ConnectionResponse is only valid if result code is \a nx_cc::ResultCode::ok
            \return \a true if async request started successfully
        */
        bool establishConnectionRequest(
            const ConnectionRequest& request,
            std::function<void(ErrorDescription, const ConnectionResponse&)>&& completionHandler );
        //!Request public IP of local peer
        /*!
            \return \a true if async request started successfully
        */
        bool getPublicIP( std::function<void(ErrorDescription, const HostAddress&)>&& completionHandler );

        static MediatorConnection* instance();
    };
}

#endif  //NX_MEDIATOR_CONNECTION_H
