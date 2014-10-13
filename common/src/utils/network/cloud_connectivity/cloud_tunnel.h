/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_TUNNEL_H
#define NX_CLOUD_TUNNEL_H

#include <functional>
#include <list>
#include <memory>

#include <utils/common/singleton.h>
#include <utils/common/stoppable.h>

#include "cc_common.h"
#include "../abstract_socket.h"


namespace nx_cc
{
    class AbstractTunnelConnector
    {
    public:
        virtual ~AbstractTunnelConnector() {}

        /*!
            \param handler void( nx_cc::ErrorDescription, std::unique_ptr<AbstractStreamSocket> )
        */
        template<class Handler> bool connectAsync( Handler handler );
        virtual HostAddress remoteHost() const = 0;

    private:
        AbstractTunnelConnector( const AbstractTunnelConnector& );
        AbstractTunnelConnector& operator=( const AbstractTunnelConnector& );
    };

    //!Implements client-side logic of setting up tunnel via mediator (with help of hole punching, probably)
    class CloudTunnelConnector
    :
        public AbstractTunnelConnector
    {
    };

    //!Implements server-side logic of setting up tunnel via mediator (with help of hole punching, probably)
    class CloudTunnelAcceptor
    :
        public AbstractTunnelConnector
    {
    };

    //!Implements established tunnel logic
    class CloudTunnelProcessor
    {
    public:
        /*!
            \param handler of type (nx_cc::ErrorDescription result, std::unique_ptr<AbstractStreamSocket> socket)
        */
        template<class Handler>
        bool createConnection( Handler&& /*handler*/ )
        {
            //TODO #ak
            return false;
        }
    };

    //TODO #ak there can be tunnels of different types:
    //    - direct or backward tcp connection (no hole punching used)
    //    - udt tunnel (no matter hole punching is used or not)
    //    - tcp with hole punching
    //Last type of tunnel does not allow us to have multiple connections per tunnel (really?)
        

    //!Tunnel between two peers. Tunnel can be established by backward TCP connection or by hole punching UDT connection
    /*!
        Tunnel connection establishment is outside of scope of this class.
        Instance of this class does not re-establish connection if it has been broken since connection establishment may 
            require some complicated steps (like going through mediator, nat traversal)
        \note It is full-duplex tunnel
        \note We use tunnel to reduce load on mediator
        \note Implements logic of existing tunnel:\n
            - keep-alive messages
            - setting up (connecting and accepting) more connections via existing tunnel
            - reconnecting (with help of \a CloudTunnelConnector or \a CloudTunnelAcceptor) in case of connection problems
        \note can create connection without mediator if tunnel is:\n
            - direct or backward tcp connection (no hole punching used)
            - udt tunnel (no matter hole punching is used or not)
    */
    class CloudTunnel
    :
        public QnStoppableAsync
    {
    public:
        /*!
            MUST call \a CloudTunnel::start after creating this object
        */
        CloudTunnel( std::unique_ptr<AbstractTunnelConnector> connector )
        :
            m_connector( std::move(connector) )
        {
        }

        //!Implementation of QnStoppableAsync::pleaseStop
        virtual void pleaseStop( std::function<void()>&& completionHandler ) override;

        /*!
            MUST call this method after creating object. This method is required because we do not use exceptions
        */
        bool start()
        {
            return m_connector->connectAsync(
                [this]( nx_cc::ErrorDescription result, std::unique_ptr<AbstractStreamSocket> socket ) {
                    tunnelEstablished( result, std::move(socket) );
                } );
        }
        //!Establish new connection
        /*!
            \param handler. void(nx_cc::ErrorDescription, std::unique_ptr<AbstractStreamSocket>)
            \note Request interleaving is allowed
        */
        template<class Handler>
        bool connect( Handler&& completionHandler )
        {
            m_connectCompletionHandler = std::forward<Handler>( completionHandler );
            //TODO
            return false;
        }
        //!Accept new incoming connection
        /*!
            \param handler. void(const ErrorDescription&, std::unique_ptr<AbstractStreamSocket>)
            //TODO #ak who listenes tunnel?
        */
        template<class Handler>
        bool acceptAsync( Handler&& completionHandler )
        {
            m_acceptCompletionHandler = std::forward<Handler>( completionHandler );
            //TODO
            return false;
        }
        //!Set handler to be called on detecting underlying connection closure
        /*!
            \param handler. void(const std::unique_ptr<CloudTunnel>&)
        */
        template<class Handler>
        void setConnectionClosedHandler( Handler&& handler )
        {
            //TODO need multiple handlers?
            m_connectionClosedHandler = std::forward<Handler>( handler );
            //TODO
        }

        HostAddress remotePeerAddress() const;

    private:
        class ConnectionRequest
        {
        public:
            std::function<void(nx_cc::ErrorDescription, std::unique_ptr<AbstractStreamSocket>)> completionHandler;
        };

        std::unique_ptr<AbstractTunnelConnector> m_connector;
        std::unique_ptr<AbstractStreamSocket> m_tunnelSocket;
        std::function<void(nx_cc::ErrorDescription, std::unique_ptr<AbstractStreamSocket>)> m_connectCompletionHandler;
        std::function<void(const ErrorDescription&, std::unique_ptr<AbstractStreamSocket>)> m_acceptCompletionHandler;
        std::function<void(const std::unique_ptr<CloudTunnel>&)> m_connectionClosedHandler;
        std::list<ConnectionRequest> m_pendingConnectionRequests;
        std::unique_ptr<CloudTunnelProcessor> m_tunnelProcessor;

        void tunnelEstablished( nx_cc::ErrorDescription result, std::unique_ptr<AbstractStreamSocket> socket );
        void processPendingConnectionRequests();
    };

    //!Stores cloud tunnels
    class CloudTunnelPool
    :
        public Singleton<CloudTunnelPool>
    {
    public:
        //!Get tunnel to \a targetHost
        /*!
            Always returns not null tunnel.
            If tunnel does not exist, allocates new tunnel object, initiates cloud connection and returns tunnel object
        */
        std::shared_ptr<CloudTunnel> getTunnelToHost( const HostAddress& targetHost ) const;
    };
}

#endif  //NX_CLOUD_TUNNEL_H
