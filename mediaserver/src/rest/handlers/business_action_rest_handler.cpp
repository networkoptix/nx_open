#include "business_action_rest_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include <business/actions/abstract_business_action.h>
#include <business/business_message_bus.h>
#include "core/resource_management/resource_pool.h"
#include "nx_ec/binary_serialization_helper.h"
#include "nx_ec/data/ec2_business_rule_data.h"

int QnBusinessActionRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    Q_UNUSED(contentType)

    resultByteArray.append("<root>\n");
    resultByteArray.append("Invalid method 'GET'. You should use 'POST' instead\n");
    resultByteArray.append("</root>\n");

    return CODE_NOT_IMPLEMETED;
}

int QnBusinessActionRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    Q_UNUSED(contentType)

    QnAbstractBusinessActionPtr action;
    ec2::ApiBusinessActionData apiData;
    InputBinaryStream<QByteArray> stream(body);
    if (deserialize(apiData, &stream))
        action = apiData.toResource(qnResPool);
    
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

QString QnBusinessActionRestHandler::description() const
{
    return "Execute business action. Action specified in POST request body in binary protobuf format. \n";
}
