#include "exec_action_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "events/abstract_business_action.h"
#include "events/business_message_bus.h"
#include <api/serializer/pb_serializer.h>

int QnExecActionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType, QByteArray& contentEncoding)
{
    Q_UNUSED(params)
    Q_UNUSED(path)

    resultByteArray.append("<root>\n");
    resultByteArray.append("Invalid method 'GET'. You should use 'POST' instead\n");
    resultByteArray.append("</root>\n");

    return CODE_NOT_IMPLEMETED;
}

int QnExecActionHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType, QByteArray& contentEncoding)
{
    QnAbstractBusinessActionPtr action;

    QnApiPbSerializer serializer;
    serializer.deserializeBusinessAction(action, body);
    if (action) {
        action->setReceivedFromRemoteHost(true);
        qnBusinessMessageBus->at_actionReceived(action);
    }

    result.append("<root>");
    result.append(action ? "OK" : "Can't deserialize action from body");
    result.append("</root>");
    return action ? CODE_OK : CODE_INVALID_PARAMETER;
}

QString QnExecActionHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    return "Execute business action. Action specified in POST request body at binary protobuf format. \n";
}
