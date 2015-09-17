#include <common/common_globals.h>
#include <utils/network/stun/cc/custom_stun.h>

namespace nx {
namespace stun {
namespace cc {
namespace attrs {

PublicEndpointList::PublicEndpointList( const std::list< SocketAddress >& endpoints )
    : stun::attrs::Unknown( TYPE )
{
    // TODO: use binary serialization instead
    QStringList list;
    for( const auto& ep : endpoints )
        list << ep.toString();

    value = list.join( lit(",") ).toUtf8();
}

std::list< SocketAddress > PublicEndpointList::get() const
{
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
