#include <common/common_globals.h>
#include <utils/network/stun/cc/custom_stun.h>

namespace nx {
namespace stun {
namespace cc {
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

PublicEndpointList::PublicEndpointList( const std::list< SocketAddress >& endpoints )
    : StringAttribute( TYPE, endpointsToString( endpoints ) )
{
}

std::list< SocketAddress > PublicEndpointList::get() const
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
