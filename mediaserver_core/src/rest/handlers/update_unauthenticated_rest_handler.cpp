#include "update_unauthenticated_rest_handler.h"

#include <media_server/server_update_tool.h>
#include <api/model/upload_update_reply.h>
#include <managers/updates_manager.h>

int QnUpdateUnauthenticatedRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* processor)
{
    return executePost(path, params, QByteArray(), result, processor);
}

int QnUpdateUnauthenticatedRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& params,
    const QByteArray& /*body*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* /*processor*/)
{
    const auto updateId = params.value(lit("updateId"));
    const bool delayed = params.value(lit("delayed"), lit("false")) != lit("false");

    if (updateId.isEmpty())
    {
        result.setError(QnJsonRestResult::InvalidParameter, lit("updateId"));
        return nx_http::StatusCode::ok;
    }

    QnUploadUpdateReply reply;
    reply.offset = ec2::AbstractUpdatesManager::NoError;

    if (delayed)
        QnServerUpdateTool::instance()->installUpdateDelayed(updateId);
    else if (!QnServerUpdateTool::instance()->installUpdate(updateId))
        reply.offset = ec2::AbstractUpdatesManager::UnknownError;

    result.setReply(reply);

    return nx_http::StatusCode::ok;
}
