/**********************************************************
* Dec 25, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_MESSAGE_DISPATCHER_H
#define NX_MEDIATOR_MESSAGE_DISPATCHER_H

#include <functional>

#include <nx/network/stun/message_dispatcher.h>

#include "connection.h"


namespace nx {
namespace hpm {

class MessageDispatcher
:
    public nx::stun::MessageDispatcher
{
public:
    /** Overrides nx::stun::MessageDispatcher::dispatchRequest.
        Translates \a nx::stun::ServerConnection to \a AbstractServerConnection 
            to allow requests received with different protocols (tcp and udp)
     */
    virtual bool dispatchRequest(
        std::shared_ptr< nx::stun::ServerConnection > connection,
        nx::stun::Message message) const override;

    /**
        \return \a false if processor for \a method has already been registered
     */
    bool registerRequestProcessor(
        int method,
        std::function<void(
            std::shared_ptr<AbstractServerConnection>,
            stun::Message)> processor);
};

}   //hpm
}   //nx

#endif  //NX_MEDIATOR_MESSAGE_DISPATCHER_H
