/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#include "stun_message_dispatcher.h"

#include <atomic>


static std::atomic<STUNMessageDispatcher*> STUNMessageDispatcher_instance;

STUNMessageDispatcher::STUNMessageDispatcher()
{
    assert( STUNMessageDispatcher_instance.load() == nullptr );
    STUNMessageDispatcher_instance.store( this, std::memory_order_relaxed );
}

STUNMessageDispatcher::~STUNMessageDispatcher()
{
    assert( STUNMessageDispatcher_instance.load() == this );
    STUNMessageDispatcher_instance.store( nullptr, std::memory_order_relaxed );
}

STUNMessageDispatcher* STUNMessageDispatcher::instance()
{
    return STUNMessageDispatcher_instance.load( std::memory_order_relaxed );
}
