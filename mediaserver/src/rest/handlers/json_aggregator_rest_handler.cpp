#include "json_aggregator_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include "rest/server/rest_connection_processor.h"

bool QnJsonAggregatorRestHandler::executeCommad(const QString &command, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner, QVariantMap& fullData)
{
    QnRestRequestHandlerPtr handler = QnRestProcessorPool::instance()->findHandler(command);
    if (!handler) {
        result.setError(QnJsonRestResult::InvalidParameter);
        result.setErrorString(lit("Rest handler '%1' is not found").arg(command));
        return false;
    }
    
    QSharedPointer<QnJsonRestHandler> jsonHandler = handler.dynamicCast<QnJsonRestHandler>();
    if (!jsonHandler) {
        result.setError(QnJsonRestResult::InvalidParameter);
        result.setErrorString(lit("Rest handler '%1' is not json handler. It is not supported").arg(command));
        return false;
    }
    QByteArray msgBody;
    QnJsonRestResult subResult;
    jsonHandler->executeGet(command, params, subResult, owner);
    QVariantMap subResultWithErr;
    subResultWithErr["error"] = subResult.error();
    subResultWithErr["errorString"] = subResult.errorString();
    subResultWithErr["reply"] = subResult.reply().toVariant();
    fullData[command] = subResultWithErr;
    return true;
}

int QnJsonAggregatorRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType, const QnRestConnectionProcessor* owner) 
{
    QnJsonRestResult jsonResult;
    int returnCode = executeGet(path, params, jsonResult, owner);
    result = QJson::serialized(jsonResult);
    contentType = "application/json";
    return returnCode;
}

int QnJsonAggregatorRestHandler::executeGet(const QString &, const QnRequestParamList & params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) 
{
    QnRequestParams outParams;
    QString cmdToExecute;
    QVariantMap fullData;

    for (auto itr = params.begin(); itr != params.end(); ++itr)
    {
        if (itr->first == "exec_cmd") {
            if (!cmdToExecute.isEmpty()) {
                if (!executeCommad(cmdToExecute, outParams, result, owner, fullData))
                    return CODE_OK;
            }
            outParams.clear();
            cmdToExecute = itr->second;
        }
        else
            outParams.insert(itr->first, itr->second);
    }
    if (!cmdToExecute.isEmpty()) {
        if (!executeCommad(cmdToExecute, outParams, result, owner, fullData))
            return CODE_OK;
    }
    result.setReply(QJsonValue::fromVariant(fullData));
    return CODE_OK;
}

int QnJsonAggregatorRestHandler::executePost(const QString &, const QnRequestParamList &, const QByteArray &, const QByteArray& , QByteArray&, QByteArray&, const QnRestConnectionProcessor*)
{
    return CODE_NOT_IMPLEMETED;
}
