#include "stun_extension_types.h"

namespace nx {
namespace network {
namespace stun {
namespace extension {

namespace methods {

nx::String toString(Value val)
{
    switch (val)
    {
        case ping:
            return "ping";
        case bind:
            return "bind";
        case listen:
            return "listen";
        case connectionAck:
            return "connectionAck";
        case resolvePeer:
            return "resolvePeer";
        case resolveDomain:
            return "resolveDomain";
        case connect:
            return "connect";
        case connectionResult:
            return "connectionResult";
        case udpHolePunchingSyn:
            return "udpHolePunchingSyn";
        case tunnelConnectionChosen:
            return "tunnelConnectionChosen";
        default:
            return "unknown";
    };
}

} // namespace methods

namespace attrs {

const char* toString(AttributeType val)
{
    switch (val)
    {
        case resultCode:
            return "resultCode";
        case systemId:
            return "systemId";
        case serverId:
            return "serverId";
        case peerId:
            return "peerId";
        case connectionId:
            return "connectionId";
        case cloudConnectVersion:
            return "cloudConnectVersion";

        case hostName:
            return "hostName";
        case hostNameList:
            return "hostNameList";
        case publicEndpointList:
            return "publicEndpointList";
        case tcpHpEndpointList:
            return "tcpHpEndpointList";
        case udtHpEndpointList:
            return "udtHpEndpointList";
        case connectionMethods:
            return "connectionMethods";
        case ignoreSourceAddress:
            return "ignoreSourceAddress";
        case tcpReverseEndpointList:
            return "tcpReverseEndpointList";
        case isPersistent:
            return "isPersistent";
        case isListening:
            return "isListening";
        case trafficRelayUrl:
            return "trafficRelayUrl";
        case trafficRelayUrlList:
            return "trafficRelayUrlList";

        case udpHolePunchingResultCode:
            return "udpHolePunchingResultCode";
        case rendezvousConnectTimeout:
            return "rendezvousConnectTimeout";
        case udpTunnelKeepAliveInterval:
            return "udpTunnelKeepAliveInterval";
        case udpTunnelKeepAliveRetries:
            return "udpTunnelKeepAliveRetries";
        case tcpReverseRetryMaxCount:
            return "tcpReverseRetryMaxCount";
        case tcpReverseRetryInitialDelay:
            return "tcpReverseRetryInitialDelay";
        case tcpReverseRetryDelayMultiplier:
            return "tcpReverseRetryDelayMultiplier";
        case tcpReverseRetryMaxDelay:
            return "tcpReverseRetryMaxDelay";
        case tcpReverseHttpSendTimeout:
            return "tcpReverseHttpSendTimeout";
        case tcpReverseHttpReadTimeout:
            return "tcpReverseHttpReadTimeout";
        case tcpReverseHttpMsgBodyTimeout:
            return "tcpReverseHttpMsgBodyTimeout";
        case tunnelInactivityTimeout:
            return "tunnelInactivityTimeout";
        case tcpConnectionKeepAlive:
            return "tcpConnectionKeepAlive";

        case udpHolePunchingStartDelay:
            return "udpHolePunchingStartDelay";
        case trafficRelayingStartDelay:
            return "trafficRelayingStartDelay";
        case directTcpConnectStartDelay:
            return "directTcpConnectStartDelay";

        case systemErrorCode:
            return "systemErrorCode";

        default:
            return "unknown";
    }
} // namespace attrs


BaseStringAttribute::BaseStringAttribute( int userType, const String& value )
    : stun::attrs::Unknown( userType, stringToBuffer( value ) )
{
}

static String endpointsToString( const std::list< SocketAddress >& endpoints )
{
    QStringList list;
    for( const auto& ep : endpoints )
        list << ep.toString();

    return list.join( lit(",") ).toUtf8();
}

//-------------------------------------------------------------------------------------------------

Endpoint::Endpoint(int type, const SocketAddress& endpoint)
    : BaseStringAttribute(type, endpoint.toString().toUtf8())
{
}

SocketAddress Endpoint::get() const
{
    return SocketAddress(getString());
}

//-------------------------------------------------------------------------------------------------

EndpointList::EndpointList( int type, const std::list< SocketAddress >& endpoints )
    : BaseStringAttribute( type, endpointsToString( endpoints ) )
{
}

std::list< SocketAddress > EndpointList::get() const
{
    const auto value = getString();
    if( value.isEmpty() )
        return std::list< SocketAddress >();

    std::list< SocketAddress > list;
    for( const auto ep : QString::fromUtf8( value ).split( lit(",") ) )
        list.push_back( SocketAddress( ep ) );

    return list;
}

static String vectorToString( const std::vector< String >& vector )
{
    QStringList list;
    for( const auto& it : vector )
        list << QString::fromUtf8( it );

    return list.join( lit(",") ).toUtf8();
}

StringList::StringList( int type, const std::vector< String >& strings )
    : BaseStringAttribute( type, vectorToString( strings ) )
{
}

std::vector< String > StringList::get() const
{
    const auto value = getString();
    if( value.isEmpty() )
        return std::vector< String >();

    std::vector< String > list;
    for( const auto it : QString::fromUtf8( value ).split( lit(",") ) )
        list.push_back( it.toUtf8() );

    return list;
}

} // namespace attrs
} // namespace extension
} // namespace stun
} // namespace network
} // namespace nx
