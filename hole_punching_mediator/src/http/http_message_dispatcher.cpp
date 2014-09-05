/**********************************************************
* 4 sep 2014
* a.kolesnikov
***********************************************************/

#include "http_message_dispatcher.h"

#include <atomic>


namespace nx_http
{
    static std::atomic<MessageDispatcher*> MessageDispatcher_instance;

    MessageDispatcher::MessageDispatcher()
    {
        assert( MessageDispatcher_instance.load() == nullptr );
        MessageDispatcher_instance.store( this, std::memory_order_relaxed );
    }

    MessageDispatcher::~MessageDispatcher()
    {
        assert( MessageDispatcher_instance.load() == this );
        MessageDispatcher_instance.store( nullptr, std::memory_order_relaxed );
    }

    MessageDispatcher* MessageDispatcher::instance()
    {
        return MessageDispatcher_instance.load( std::memory_order_relaxed );
    }
}
