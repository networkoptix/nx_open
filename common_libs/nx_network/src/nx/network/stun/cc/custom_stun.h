/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CUSTOM_STUN_H
#define NX_CUSTOM_STUN_H

#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace stun {
namespace cc {

namespace methods
{
    enum Value
    {
        /** Pings mediaserver endpoints and reports verified onese
         *  Request: SystemId, ServerId, PublicEndpointList
         *  Response: PublicEndpointList (vrified) */
        ping = stun::MethodType::userMethod,

        /** Registers mediaserver for external connections.
         *  Request: SystemId, ServerId, PublicEndpointList */
        bind,

        // TODO: specify bahavior
        listen,

        resolve,    //!< Returns host's public address list and suitable connections methods

        /** Initiate connection to some mediaserver
         *  Request: \class PeerId, \class HostName, \class ConnectionId
         *  Response: \class PublicEndpointList (opt), \class TcpHpEndpointList (opt),
         *            \class UdtHpEndpointList (opt)
         */
        connect
    };

    nx::String toString(Value val);
}

namespace indications
{
    enum Value
    {
        /** Indicates requested connection
         *  Attrs: \class PeerId, \class ConnectionId,
         *         \class PublicEndpointList (opt), \class TcpHpEndpointList (opt),
         *         \class UdtHpEndpointList (opt)
         */
        connectionRequested,

        /** Indicates update information about on-going connection establishment
         *  Attrs: \class ConnectionId,
         *         \class PublicEndpointList (opt), \class TcpHpEndpointList (opt),
         *         \class UdtHpEndpointList (opt)
         */
        connectionUpdate
    };
}

namespace error
{
    enum Value
    {
        notFound = 404,
    };
}

//TODO custom stun requests parameters
namespace attrs
{
    enum Value
    {
        systemId = stun::attrs::userDefined,
        serverId,
        peerId,
        connectionId,

        hostName = stun::attrs::userDefined + 0x200,
        publicEndpointList,
        tcpHpEndpointList,
        udtHpEndpointList,
        connectionMethods,
    };


    /** Base class for string attributes */
    struct NX_NETWORK_API StringAttribute : stun::attrs::Unknown
    {
        StringAttribute( int userType, const String& value = String() );
    };

    struct NX_NETWORK_API SystemId : StringAttribute
    {
        static const int TYPE = systemId;
        SystemId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ServerId : StringAttribute
    {
        static const int TYPE = serverId;
        ServerId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API PeerId : StringAttribute
    {
        static const int TYPE = peerId;
        PeerId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ConnectionId : StringAttribute
    {
        static const int TYPE = connectionId;
        ConnectionId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API HostName : StringAttribute
    {
        static const int TYPE = hostName;
        HostName( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ConnectionMethods: StringAttribute
    {
        static const int TYPE = connectionMethods;
        ConnectionMethods(const String& value): StringAttribute(TYPE, value) {}
    };
    


    /** Base class for endpoint attributes */
    struct NX_NETWORK_API EndpointList : StringAttribute
    {
        EndpointList( int type, const std::list< SocketAddress >& endpoints );
        std::list< SocketAddress > get() const;
    };

    struct NX_NETWORK_API PublicEndpointList : EndpointList
    {
        static const int TYPE = publicEndpointList;
        PublicEndpointList( const std::list< SocketAddress >& endpoints )
            : EndpointList( TYPE, endpoints ) {}
    };

    struct NX_NETWORK_API TcpHpEndpointList : EndpointList
    {
        static const int TYPE = tcpHpEndpointList;
        TcpHpEndpointList( const std::list< SocketAddress >& endpoints )
            : EndpointList( TYPE, endpoints ) {}
    };

    struct NX_NETWORK_API UdtHpEndpointList : EndpointList
    {
        static const int TYPE = udtHpEndpointList;
        UdtHpEndpointList( const std::list< SocketAddress >& endpoints )
            : EndpointList( TYPE, endpoints ) {}
    };
}

} // namespace cc
} // namespace stun
} // namespace nx

#endif  //NX_CUSTOM_STUN_H
