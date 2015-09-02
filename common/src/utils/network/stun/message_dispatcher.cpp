/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#include "message_dispatcher.h"

namespace nx {
namespace stun {

bool MessageDispatcher::registerRequestProcessor(
        int method, MessageProcessor processor )
{
    return m_processors.emplace( method, std::move(processor) ).second;
}

bool MessageDispatcher::dispatchRequest(
        const std::shared_ptr< ServerConnection >& connection,
        stun::Message message )
{
    const auto it = m_processors.find( message.header.method );
    if( it == m_processors.end() )
        return false;

    it->second( connection, std::move( message ) );
    return true;
}

} // namespace stun
} // namespace nx
