/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_DISPATCHER_H
#define STUN_MESSAGE_DISPATCHER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include <utils/common/singleton.h>
#include <utils/common/stoppable.h>
#include <utils/network/connection_server/message_dispatcher.h>

#include "stun_server_connection.h"

namespace nx {
namespace hpm {

//!Dispatches STUN protocol messages to corresponding processor
/*!
    \note This class methods are not thread-safe
*/
class STUNMessageDispatcher
:
    public Singleton<STUNMessageDispatcher>
{
public:
    typedef std::function<bool(StunServerConnection*, stun::Message&&)> MessageProcessorType;

    /*!
        \param messageProcessor Ownership of this object is not passed
        \note All processors MUST be registered before connection processing is started, since this class methods are not thread-safe
        \return \a true if \a requestProcessor has been registered, \a false otherwise
        \note Message processing function MUST be non-blocking
    */
    bool registerRequestProcessor(
        int method,
        MessageProcessorType&& messageProcessor )
    {
        return m_processors.emplace( method, messageProcessor ).second;
    }
    //!Pass message to corresponding processor
    /*!
        \param message This object is not moved in case of failure to find processor
        \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
    */
    bool dispatchRequest(
        StunServerConnection* connection,
        stun::Message&& message )
    {
        auto it = m_processors.find( message.header.method );
        if( it == m_processors.end() )
            return false;
        return it->second(
            connection,
            std::move( message ) );
    }

private:
    std::unordered_map<int, MessageProcessorType> m_processors;
};

} // namespace hpm
} // namespace nx

#endif  //STUN_MESSAGE_DISPATCHER_H
