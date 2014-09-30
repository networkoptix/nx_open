/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_DISPATCHER_H
#define STUN_MESSAGE_DISPATCHER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include <utils/common/stoppable.h>

#include "stun_server_connection.h"
#include "../message_dispatcher.h"


//!Dispatches STUN protocol messages to corresponding processor
/*!
    \note This class methods are not thread-safe
*/
class STUNMessageDispatcher
:
    public
        MessageDispatcher<
            StunServerConnection,
            nx_stun::Message,
            typename std::unordered_map<int, typename std::function<bool(StunServerConnection*, nx_stun::Message&&)> > >

{
    typedef
        MessageDispatcher<
            StunServerConnection,
            nx_stun::Message,
            typename std::unordered_map<int, typename std::function<bool(StunServerConnection*, nx_stun::Message&&)> > > base_type;
public:
    STUNMessageDispatcher();
    virtual ~STUNMessageDispatcher();

    //!Pass message to corresponding processor
    /*!
        \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
    */
    bool dispatchRequest( StunServerConnection* conn, nx_stun::Message&& message )
    {
        return base_type::dispatchRequest( conn, message.header.method, std::move(message) );
    }

    static STUNMessageDispatcher* instance();
};

#endif  //STUN_MESSAGE_DISPATCHER_H
