/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#include "stun_message_dispatcher.h"

namespace nx {
namespace hpm {

bool STUNMessageDispatcher::registerRequestProcessor(
        int method, MessageProcessorType&& messageProcessor )
{
    return m_processors.emplace( method, std::move(messageProcessor) ).second;
}

bool STUNMessageDispatcher::dispatchRequest(
        StunServerConnection* connection, stun::Message&& message )
{
    const auto it = m_processors.find( message.header.method );
    if( it == m_processors.end() )
        return false;

    return it->second( connection, std::move( message ) );
}

} // namespace hpm
} // namespace nx
