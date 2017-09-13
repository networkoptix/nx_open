#include "event_log_rest_handler.h"

#include <api/helpers/event_log_request_data.h>

#include <database/server_db.h>

#include <rest/server/rest_connection_processor.h>

int QnEventLogRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QnEventLogRequestData request;
    request.loadFromParams(owner->resourcePool(), params);

    QString errStr;
    if (request.isValid(&errStr))
    {
        qnServerDb->getAndSerializeActions(request, result);
        contentType = "application/octet-stream";
        return nx_http::StatusCode::ok;
    }
    else
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return nx_http::StatusCode::invalidParameter;
    }
}

int QnEventLogRestHandler::executePost(
    const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/, QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
