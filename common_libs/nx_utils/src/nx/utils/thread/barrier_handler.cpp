#include "barrier_handler.h"

namespace nx {

BarrierHandler::BarrierHandler( std::function< void() > handler )
    // TODO: move lambda
    : m_handlerHolder( this, [ handler ]( BarrierHandler* ){ handler(); } )
{
}

std::function< void() > BarrierHandler::fork()
{
    // TODO: move lambda
    auto holder = std::make_shared<
            std::shared_ptr< BarrierHandler > >( m_handlerHolder );

    return [ holder ](){ holder->reset(); };
}

} // namespace nx
