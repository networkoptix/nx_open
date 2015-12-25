/**********************************************************
* Dec 25, 2015
* akolesnikov
***********************************************************/

#include "message_dispatcher.h"

#include "connection.h"


namespace nx {
namespace hpm {

bool MessageDispatcher::dispatchRequest(
    std::shared_ptr< nx::stun::ServerConnection > connection,
    nx::stun::Message message) const
{
    return nx::stun::MessageDispatcher::dispatchRequest(
        std::move(connection),
        message);
}

bool MessageDispatcher::registerRequestProcessor(
    int method,
    std::function<void(
        std::shared_ptr<AbstractServerConnection>,
        stun::Message)> processor)
{
    //TODO #ak we MUST dispatch UDP requests too

    return nx::stun::MessageDispatcher::registerRequestProcessor(
        method,
        [/*std::move*/ processor](
            std::shared_ptr< nx::stun::ServerConnection > connection,
            stun::Message message)
        {
            auto tcpConnectionWrapper = TcpServerConnection::create(std::move(connection));
            processor(
                std::move(tcpConnectionWrapper),
                std::move(message));
        });
}

}   //hpm
}   //nx
