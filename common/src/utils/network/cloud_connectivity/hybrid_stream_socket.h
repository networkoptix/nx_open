/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_HYBRID_STREAM_SOCKET_H
#define NX_HYBRID_STREAM_SOCKET_H

#include <memory>

#include "../abstract_socket.h"
#include "cc_common.h"
#include "cloud_tunnel.h"
#include "dns_table.h"


namespace nx_cc
{
    //!Socket that is able to use hole punching (tcp or udp) and mediator to establish connection
    /*!
        Method to use to connect to remote peer is selected depending on route to the peer
        If connection to peer requires using udp hole punching than this socket uses UDT.
        \note Actual socket is instanciated only when address is known (\a AbstractCommunicatingSocket::connect or \a AbstractCommunicatingSocket::connectAsync)
    */
    class CloudStreamSocket
    :
        public AbstractStreamSocket
    {
    public:
        //TODO #ak add all socket functions

        //!Implementation of AbstractStreamSocket::connect
        virtual bool connect(
            const SocketAddress& remoteAddress,
            unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;

    protected:
        //!Implementation of AbstractStreamSocket::connectAsyncImpl
        virtual bool connectAsyncImpl(
            const SocketAddress& addr,
            std::function<void( SystemError::ErrorCode )>&& handler ) override;

    private:
        std::unique_ptr<AbstractStreamSocket> m_socketDelegate;
        std::function<void( SystemError::ErrorCode )> m_connectHandler;

        void applyCachedAttributes();
        bool instanciateSocket( const nx_cc::DnsEntry& dnsEntry );
        void onResolveDone( std::vector<nx_cc::DnsEntry> dnsEntries );
        bool startAsyncConnect(
            std::vector<nx_cc::DnsEntry>&& dnsEntries,
            int port );
        void cloudConnectDone(
            std::shared_ptr<CloudTunnel> tunnel,
            nx_cc::ErrorDescription errorCode,
            std::unique_ptr<AbstractStreamSocket> cloudConnection );
    };
}

#endif  //NX_HYBRID_STREAM_SOCKET_H
