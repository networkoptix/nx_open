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
    QVariantMap& fullData,
    std::chrono::milliseconds timeout)
{
    int port = owner->owner()->getPort();
    nx::utils::Url url(lit("https://localhost:%1/%2").arg(port).arg(QnTcpListener::normalizedPath(command)));
    QUrlQuery urlQuery;
    for (auto itr = params.begin(); itr != params.end(); ++itr)
        urlQuery.addQueryItem(itr.key(), itr.value());
    url.setQuery(urlQuery);

    auto server = owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
    if (!server)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, "Internal server error: No server resource");
        return false;
    }

    nx::network::http::HttpClient client;
    client.setUserName(server->getId().toString());
    client.setUserPassword(server->getAuthKey());
    client.setSendTimeout(timeout);
    client.setResponseReadTimeout(timeout);
    client.setMessageBodyReadTimeout(timeout);
    if (auto user = owner->resourcePool()->getResourceById<QnUserResource>(owner->accessRights().userId))
        client.addAdditionalHeader(Qn::CUSTOM_USERNAME_HEADER_NAME, user->getName().toUtf8());

    if (!client.doGet(url) || !client.response())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lm("%1 has failed: %2").args(
            command, SystemError::toString(client.lastSysErrorCode())));
        return false;
    }

    nx::network::http::BufferType msgBody;
    while (!client.eof())
        msgBody.append(client.fetchMessageBodyBuffer());

    const auto status = client.response()->statusLine;
    if (!nx::network::http::StatusCode::isSuccessCode(status.statusCode))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lm("%1 has failed: %2 %3").args(
            command, status.statusCode, status.reasonPhrase));
        return false;
    }

    if (!client.contentType().toLower().contains("json"))
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lm("%1 has failed: Response content type is not JSON").args(command));
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msgBody, &error);
    if (error.error != QJsonParseError::NoError)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lm("%1 has failed: Response parse error: %2").args(command, error.errorString()));
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
    std::chrono::seconds timeout(10);

    for (auto itr = params.begin(); itr != params.end(); ++itr)
    {
        if (itr->first == "exec_cmd")
        {
            if (!cmdToExecute.isEmpty())
            {
                if (!executeCommad(cmdToExecute, outParams, result, owner, fullData, timeout))
                    return nx::network::http::StatusCode::ok;
            }
            outParams.clear();
            cmdToExecute = itr->second;
        }
        else if (itr->first == "exec_timeout")
        {
            timeout = std::chrono::seconds(itr->second.toInt());
        }
        else
        {
            outParams.insert(itr->first, itr->second);
        }
    }
    if (!cmdToExecute.isEmpty())
    {
        if (!executeCommad(cmdToExecute, outParams, result, owner, fullData, timeout))
            return nx::network::http::StatusCode::ok;
    }
    result.setReply(QJsonValue::fromVariant(fullData));
    return nx::network::http::StatusCode::ok;
}

int QnJsonAggregatorRestHandler::executePost(const QString &, const QnRequestParamList &, const QByteArray &, const QByteArray& , QByteArray&, QByteArray&, const QnRestConnectionProcessor*)
{
    return nx::network::http::StatusCode::notImplemented;
}
