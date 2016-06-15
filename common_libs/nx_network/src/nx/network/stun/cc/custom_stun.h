/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CUSTOM_STUN_H
#define NX_CUSTOM_STUN_H

#include <nx/network/cloud/data/result_code.h>
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

        /** Notifies mediator that server is ready to accept cloud connections */
        listen,

        /** server uses this request to confirm its willingness to proceed with cloud connection */
        connectionAck,

        /** Returns host name list included in domain */
        resolveDomain,

        /** Returns host's public address list and suitable connections methods */
        resolvePeer,

        /** Initiate connection to some mediaserver
         *  Request: \class PeerId, \class HostName, \class ConnectionId
         *  Response: \class PublicEndpointList (opt), \class TcpHpEndpointList (opt),
         *            \class UdtHpEndpointList (opt)
         */
        connect,
        connectionResult,
        udpHolePunchingSyn,
        tunnelConnectionChosen,
    };

    NX_NETWORK_API nx::String toString(Value val);
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
    enum AttributeType
    {
        resultCode = stun::attrs::userDefined,

        systemId,
        serverId,
        peerId,
        connectionId,
        cloudConnectVersion,

        hostName = stun::attrs::userDefined + 0x200,
        hostNameList,
        publicEndpointList,
        tcpHpEndpointList,
        udtHpEndpointList,
        connectionMethods,
        ignoreSourceAddress,

        udpHolePunchingResultCode = stun::attrs::userDefined + 0x400,
        rendezvousConnectTimeout,
        udpTunnelKeepAliveInterval,
        udpTunnelKeepAliveRetries,

        systemErrorCode = stun::attrs::userDefined + 0x500,
    };

    NX_NETWORK_API const char* toString(AttributeType val);


    /** Base class for string attributes */
    struct NX_NETWORK_API StringAttribute : stun::attrs::Unknown
    {
        StringAttribute( int userType, const String& value = String() );
    };

    struct NX_NETWORK_API ResultCode: stun::attrs::IntAttribute
    {
        static const AttributeType TYPE = resultCode;

        ResultCode(const nx::hpm::api::ResultCode& value)
            : stun::attrs::IntAttribute(TYPE, static_cast<int>(value)) {}

        nx::hpm::api::ResultCode value() const
        {
            return static_cast<nx::hpm::api::ResultCode>(
                stun::attrs::IntAttribute::value());
        }
    };

    struct NX_NETWORK_API SystemId : StringAttribute
    {
        static const AttributeType TYPE = systemId;
        SystemId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ServerId : StringAttribute
    {
        static const AttributeType TYPE = serverId;
        ServerId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API PeerId : StringAttribute
    {
        static const AttributeType TYPE = peerId;
        PeerId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ConnectionId : StringAttribute
    {
        static const AttributeType TYPE = connectionId;
        ConnectionId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API HostName : StringAttribute
    {
        static const AttributeType TYPE = hostName;
        HostName( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ConnectionMethods: StringAttribute
    {
        static const AttributeType TYPE = connectionMethods;
        ConnectionMethods(const String& value): StringAttribute(TYPE, value) {}
    };

    struct NX_NETWORK_API UdpHolePunchingResultCodeAttr: stun::attrs::IntAttribute
    {
        static const AttributeType TYPE = udpHolePunchingResultCode;
        UdpHolePunchingResultCodeAttr(int value)
            : stun::attrs::IntAttribute(TYPE, value) {}
    };

    struct NX_NETWORK_API SystemErrorCodeAttr: stun::attrs::IntAttribute
    {
        static const AttributeType TYPE = systemErrorCode;
        SystemErrorCodeAttr(int value)
            : stun::attrs::IntAttribute(TYPE, value) {}
    };



    /** Base class for endpoint attributes */
    struct NX_NETWORK_API EndpointList : StringAttribute
    {
        EndpointList( int type, const std::list< SocketAddress >& endpoints );
        std::list< SocketAddress > get() const;
    };

    struct NX_NETWORK_API PublicEndpointList : EndpointList
    {
        static const AttributeType TYPE = publicEndpointList;
        PublicEndpointList( const std::list< SocketAddress >& endpoints )
            : EndpointList( TYPE, endpoints ) {}
    };

    struct NX_NETWORK_API TcpHpEndpointList : EndpointList
    {
        static const AttributeType TYPE = tcpHpEndpointList;
        TcpHpEndpointList( const std::list< SocketAddress >& endpoints )
            : EndpointList( TYPE, endpoints ) {}
    };

    struct NX_NETWORK_API UdtHpEndpointList : EndpointList
    {
        static const AttributeType TYPE = udtHpEndpointList;
        UdtHpEndpointList( const std::list< SocketAddress >& endpoints )
            : EndpointList( TYPE, endpoints ) {}
    };


    /** Base class for string list based attributes */
    struct NX_NETWORK_API StringList : StringAttribute
    {
        StringList( int type, const std::vector< String >& strings );
        std::vector< String > get() const;
    };

    struct NX_NETWORK_API HostNameList : StringList
    {
        static const AttributeType TYPE = hostNameList;
        HostNameList( const std::vector< String >& hostNames )
            : StringList( TYPE, hostNames ) {}
    };
}

} // namespace cc
} // namespace stun
} // namespace nx

#endif  //NX_CUSTOM_STUN_H
