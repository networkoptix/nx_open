#include "active_connections_rest_handler.h"

#include "transaction/transaction_message_bus.h"
#include <utils/common/model_functions.h>

int QnActiveConnectionsRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    QList<ec2::QnTransportConnectionInfo> connectionsInfo = qnTransactionBus->connectionsInfo();

    QJsonArray connections;
    for (const ec2::QnTransportConnectionInfo &info: connectionsInfo) {
        QJsonObject map;
        map.insert(lit("type"), info.incoming ? lit("incoming") : lit("outgoing"));
        map.insert(lit("url"), info.url.toString());
        map.insert(lit("remotePeerId"), info.remotePeerId.toString());
        map.insert(lit("state"), ec2::QnTransactionTransport::toString(info.state));
        connections.append(map);
    }

    result.setReply(connections);

    return nx_http::StatusCode::ok;
}
