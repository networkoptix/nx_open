#include "active_connections_rest_handler.h"

#include <transaction/message_bus_adapter.h>
#include <nx/fusion/model_functions.h>

namespace rest {
namespace handlers {

ActiveConnectionsRestHandler::ActiveConnectionsRestHandler(
    const ec2::AbstractTransactionMessageBus* messageBus)
:
    m_messageBus(messageBus)
{
}

nx::network::rest::Response ActiveConnectionsRestHandler::executeGet(
    const nx::network::rest::Request& /*request*/)
{
    return nx::network::rest::Response::reply(m_messageBus->connectionsInfo());
}

} // namespace handlers
} // namespace rest
