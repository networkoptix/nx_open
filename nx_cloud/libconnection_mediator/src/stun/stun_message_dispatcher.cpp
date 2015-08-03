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
    return m_processors.emplace( method, messageProcessor ).second;
}
//!Pass message to corresponding processor
/*!
    \param message This object is not moved in case of failure to find processor
    \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
*/
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
