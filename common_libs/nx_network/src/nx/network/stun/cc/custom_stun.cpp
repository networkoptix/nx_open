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
            case resolve:
                return "resolve";
            case connect:
                return "connect";
            default:
                return "unknown";
        };
    }
}

namespace attrs {

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
