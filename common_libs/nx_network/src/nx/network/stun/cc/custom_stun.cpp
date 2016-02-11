#include <common/common_globals.h>
#include <nx/network/stun/cc/custom_stun.h>

namespace nx {
namespace stun {
namespace cc {

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
            case resolve:
                return "resolve";
            case connect:
                return "connect";
            case connectionResult:
                return "connectionResult";
            case udpHolePunchingSyn:
                return "udpHolePunchingSyn";
            case udpHolePunchingSynAck:
                return "udpHolePunchingSynAck";
            default:
                return "unknown";
        };
    }
}

namespace attrs {


const char* toString(AttributeType val)
{
    switch (val)
    {
        case systemId:
            return "systemId";
        case serverId:
            return "serverId";
        case peerId:
            return "peerId";
        case connectionId:
            return "connectionId";
        case hostName:
            return "hostName";
        case publicEndpointList:
            return "publicEndpointList";
        case tcpHpEndpointList:
            return "tcpHpEndpointList";
        case udtHpEndpointList:
            return "udtHpEndpointList";
        case connectionMethods:
            return "connectionMethods";
        default:
            return "unknown";
    }
}


StringAttribute::StringAttribute( int userType, const String& value )
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

EndpointList::EndpointList( int type, const std::list< SocketAddress >& endpoints )
    : StringAttribute( type, endpointsToString( endpoints ) )
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

} // namespace attrs
} // namespace cc
} // namespace stun
} // namespace nx
