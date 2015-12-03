#include "barrier_handler.h"

namespace nx {

BarrierHandler::BarrierHandler( std::function< void() > handler )
    : m_impl( new Impl( std::move( handler ) ) )
{
    m_impl->mutex.lock(); // prevent the barier to pass
}

BarrierHandler::~BarrierHandler()
{
    if( m_impl->counter == 0 )
        m_impl->handler();

    m_impl->mutex.unlock(); // ready for callbacks
}

std::function< void() > BarrierHandler::fork()
{
    auto& impl = m_impl;
    ++impl->counter;

    return [ impl ]()
    {
        QnMutexLocker lk( &impl->mutex );
        if( --impl->counter == 0 )
            impl->handler();
    };
}

BarrierHandler::Impl::Impl( std::function< void() > handler_ )
    : counter( 0 )
    , handler( std::move( handler_ ) )
{
}

} // namespace nx
