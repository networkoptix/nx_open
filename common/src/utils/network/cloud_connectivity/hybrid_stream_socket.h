/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_HYBRID_STREAM_SOCKET_H
#define NX_HYBRID_STREAM_SOCKET_H

#include <memory>

#include "../abstract_socket.h"
#include "cc_common.h"
#include "dns_table.h"


namespace nx_cc
{
    //!Socket that dynamically selects stream protocol (tcp or udt) to use depending on route to requested peer
    /*!
        If connection to peer requires using udp hole punching than this socket uses UDT.
        \note Actual socket is instanciated only when address is known (\a AbstractCommunicatingSocket::connect or \a AbstractCommunicatingSocket::connectAsync)
    */
    class HybridStreamSocket
    :
        public AbstractStreamSocket
    {
    public:
        //!Implementation of AbstractStreamSocket::connect
        virtual bool connect(
            const QString& foreignAddress,
            unsigned short foreignPort,
            unsigned int timeoutMillis = DEFAULT_TIMEOUT_MILLIS ) override;

    protected:
        //!Implementation of AbstractStreamSocket::connectAsyncImpl
        virtual bool connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler ) override;

    private:
        std::unique_ptr<AbstractStreamSocket> m_socketDelegate;
        std::function<void( SystemError::ErrorCode )> m_connectHandler;
        SocketAddress m_remoteAddr;

        void applyCachedAttributes();
        bool instanciateSocket( const nx_cc::DnsEntry& dnsEntry );
        void onResolveDone( const std::vector<nx_cc::DnsEntry>& dnsEntries );
        void cloudConnectDone( nx_cc::ErrorDescription errorCode, AbstractStreamSocket* cloudConnection );
    };
}

#endif  //NX_HYBRID_STREAM_SOCKET_H
