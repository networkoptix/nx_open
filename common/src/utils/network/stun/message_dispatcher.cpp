/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#include "message_dispatcher.h"

namespace nx {
namespace stun {

bool MessageDispatcher::registerRequestProcessor(
        int method, MessageProcessorType&& messageProcessor )
{
    return m_processors.emplace( method, std::move(messageProcessor) ).second;
}

bool MessageDispatcher::dispatchRequest(
        ServerConnection* connection, stun::Message&& message )
{
    const auto it = m_processors.find( message.header.method );
    if( it == m_processors.end() )
        return false;

    return it->second( connection, std::move( message ) );
}

} // namespace stun
} // namespace nx
