/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_TUNNEL_H
#define NX_CLOUD_TUNNEL_H

#include <functional>

#include "cc_common.h"
#include "../abstract_socket.h"


namespace nx_cc
{
    //!Tunnel between two peers. Tunnel can be established by backward TCP connection or by hole punching UDT connection
    class CloudTunnel
    {
    public:
        virtual ~CloudTunnel();

        //!Establish new connection
        bool connect( std::function<void(nx_cc::ErrorDescription, AbstractStreamSocket*)>&& completionHandler );
        //!Accept new incoming connection
        bool acceptAsync( std::function<void(const ErrorDescription&, AbstractStreamSocket*)>&& completionHandler );

        HostAddress remotePeerAddress() const;
    };
}

#endif  //NX_CLOUD_TUNNEL_H
