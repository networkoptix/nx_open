#include "json_aggregator_rest_handler.h"

#include <common/common_module.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <rest/server/rest_connection_processor.h>

#include <network/tcp_connection_priv.h>
#include <network/tcp_listener.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/custom_headers.h>

#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

bool QnJsonAggregatorRestHandler::executeCommad(
    const QString &command,
    const QnRequestParams & params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner,
    QVariantMap& fullData)
{
    int port = owner->owner()->getPort();
    nx::utils::Url url(lit("http://localhost:%1/%2").arg(port).arg(QnTcpListener::normalizedPath(command)));
    QUrlQuery urlQuery;
    for (auto itr = params.begin(); itr != params.end(); ++itr)
        urlQuery.addQueryItem(itr.key(), itr.value());
    url.setQuery(urlQuery);

    auto server = owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
    if (!server)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error while executing request '%1'").arg(command));
        return false;
    }

    nx_http::HttpClient client;
    client.setUserName(server->getId().toString());
    client.setUserPassword(server->getAuthKey());
    auto user = owner->resourcePool()->getResourceById<QnUserResource>(owner->accessRights().userId);
    if (user)
        client.addAdditionalHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, user->getName().toUtf8());
    if (!client.doGet(url))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error while executing request '%1'").arg(command));
        return false;
    }

    if (!client.response())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't exec request '%1'").arg(command));
        return false;
    }

    nx_http::BufferType msgBody;
    while (!client.eof())
        msgBody.append(client.fetchMessageBodyBuffer());

    int statusCode = client.response()->statusLine.statusCode;
    if (statusCode / 2 != 200 && msgBody.isEmpty())
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Can't exec request '%1'. HTTP error '%2'").
            arg(command).
            arg(QString::fromUtf8(client.response()->statusLine.reasonPhrase)));
        return false;
    }

    if (!client.contentType().toLower().contains("json"))
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Request '%1' has no json content type. Only json result is supported").
            arg(command));
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msgBody, &error);
    if (error.error != QJsonParseError::NoError)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Request '%1' parse error: %2").arg(command).arg(error.errorString()));
        return false;
    }

    fullData[command] = doc.toVariant();

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
        if (itr->first == "exec_cmd")
        {
            if (!cmdToExecute.isEmpty())
            {
                if (!executeCommad(cmdToExecute, outParams, result, owner, fullData))
                    return CODE_OK;
            }
            outParams.clear();
            cmdToExecute = itr->second;
        }
        else
        {
            outParams.insert(itr->first, itr->second);
        }
    }
    if (!cmdToExecute.isEmpty())
    {
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
