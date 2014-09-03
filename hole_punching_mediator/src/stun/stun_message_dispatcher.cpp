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

void STUNMessageDispatcher::pleaseStop( std::function<void()>&& /*completionHandler*/ )
{
    //TODO #ak
}

bool STUNMessageDispatcher::registerRequestProcessor( int method, MessageProcessorType&& messageProcessor )
{
    return m_processors.emplace( method, messageProcessor ).second;
}

bool STUNMessageDispatcher::dispatchRequest( const std::weak_ptr<StunServerConnection>& conn, nx_stun::Message&& message )
{
    auto it = m_processors.find( message.header.method );
    if( it == m_processors.end() )
        return false;
    return it->second( conn, std::move(message) );
}

STUNMessageDispatcher* STUNMessageDispatcher::instance()
{
    return STUNMessageDispatcher_instance.load( std::memory_order_relaxed );
}
