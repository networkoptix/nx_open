#include "update_unauthenticated_rest_handler.h"

#include <media_server/server_update_tool.h>
#include <api/model/upload_update_reply.h>
#include <managers/updates_manager.h>
#include <nx/utils/file_system.h>

QnUpdateUnauthenticatedRestHandler::QnUpdateUnauthenticatedRestHandler(QnServerUpdateTool* updateTool):
    m_updateTool(updateTool)
{
}

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

    // TODO: updateId is not a path, so verification must be a bit tougher.
    // It may be verified to be a file name.
    if (updateId.isEmpty() || !nx::utils::file_system::isRelativePathSafe(updateId))
    {
        result.setError(QnJsonRestResult::InvalidParameter, lit("updateId"));
        return nx::network::http::StatusCode::ok;
    }

    QnUploadUpdateReply reply;
    const bool res = m_updateTool->installUpdate(
        updateId,
        delayed
            ? QnServerUpdateTool::UpdateType::Delayed
            : QnServerUpdateTool::UpdateType::Instant);

    reply.offset = res
        ? ec2::AbstractUpdatesManager::NoError
        : ec2::AbstractUpdatesManager::UnknownError;
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}
