#include "custom_stun.h"

#include <common/common_globals.h>

namespace nx {
namespace hpm {
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
    std::list< SocketAddress > list;
    for( const auto ep : QString::fromUtf8( value ).split( lit(",") ) )
        list.push_back( SocketAddress( ep ) );
    return list;
}

} // namespace attrs
} // namespace hpm
} // namespace nx
