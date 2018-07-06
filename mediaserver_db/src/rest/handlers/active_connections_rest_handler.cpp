#include "active_connections_rest_handler.h"

#include <transaction/message_bus_adapter.h>
#include <nx/fusion/model_functions.h>

QnActiveConnectionsRestHandler::QnActiveConnectionsRestHandler(
    const ec2::AbstractTransactionMessageBus* messageBus):
    QnJsonRestHandler(),
    m_messageBus(messageBus)
{
}

int QnActiveConnectionsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* /*owner*/)
{
    result.setReply(m_messageBus->connectionsInfo());
    return nx::network::http::StatusCode::ok;
}
