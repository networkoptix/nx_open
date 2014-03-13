#include "exec_action_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include <business/actions/abstract_business_action.h>
#include <business/business_message_bus.h>
#include <api/serializer/pb_serializer.h>
#include "core/resource_management/resource_pool.h"

int QnExecActionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    Q_UNUSED(contentType)

    resultByteArray.append("<root>\n");
    resultByteArray.append("Invalid method 'GET'. You should use 'POST' instead\n");
    resultByteArray.append("</root>\n");

    return CODE_NOT_IMPLEMETED;
}

int QnExecActionHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    Q_UNUSED(contentType)

    QnAbstractBusinessActionPtr action;

    QnApiPbSerializer serializer;
    serializer.deserializeBusinessAction(action, body);
    if (action) {
        action->setReceivedFromRemoteHost(true);

        QString resId;
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i].first == "resource")
                resId = params[i].second;
        }

        QnResourcePtr res = qnResPool->getResourceById(resId);
        qnBusinessMessageBus->at_actionReceived(action, res);
    }

    result.append("<root>");
    result.append(action ? "OK" : "Can't deserialize action from body");
    result.append("</root>");
    return action ? CODE_OK : CODE_INVALID_PARAMETER;
}

QString QnExecActionHandler::description() const
{
    return "Execute business action. Action specified in POST request body in binary protobuf format. \n";
}
