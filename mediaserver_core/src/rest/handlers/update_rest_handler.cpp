#include "update_rest_handler.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>

#include <api/model/upload_update_reply.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <managers/updates_manager.h>
#include <media_server/server_update_tool.h>
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>
#include <utils/update/update_utils.h>

#include <nx/utils/file_system.h>
#include <nx/utils/log/log.h>
#include <common/static_common_module.h>

QnUpdateRestHandler::QnUpdateRestHandler(QnServerUpdateTool* updateTool):
    m_updateTool(updateTool)
{
}

int QnUpdateRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor *processor)
{
    return executePost(path, params, QByteArray(), result, processor);
}

int QnUpdateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& params,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* processor)
{
    bool remotePeerHasAdminRights = processor->resourceAccessManager()->hasGlobalPermission(
        processor->accessRights(),
        GlobalPermission::admin);

    if (!remotePeerHasAdminRights)
    {
        QnUploadUpdateReply reply;
        reply.offset = ec2::AbstractUpdatesManager::UnknownError;
        result.setError(QnJsonRestResult::Error::Forbidden);
        result.setReply(reply);
        return nx::network::http::StatusCode::forbidden;
    }

    qint64 offset = params.value(lit("offset")).toLongLong();
    QString updateId = params.value(lit("updateId"));
    bool delayed = params.value(lit("delayed"), lit("false")) != lit("false");

    if (!updateId.isEmpty()) {
        if (params.contains(lit("offset"))) {
            return handlePartialUpdate(updateId, body, offset, result);
        } else if (body.isEmpty()) {
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
    }

    // TODO: updateId is not a path, so verification must be a bit tougher.
    // It may be verified to be a file name.
    if (!nx::utils::file_system::isRelativePathSafe(updateId))
    {
        result.setError(QnRestResult::InvalidParameter, lit("updateId"));
        return nx::network::http::StatusCode::ok;
    }

    nx::utils::SoftwareVersion version;
    nx::vms::api::SystemInformation sysInfo;

    {
        QBuffer buffer(const_cast<QByteArray*>(&body)); // we're going to read data, so const_cast is safe
        if (!verifyUpdatePackage(&buffer, &version, &sysInfo)) {
            NX_WARNING(this, "An upload has been received but not veryfied");
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INVALID_FILE"));
            return nx::network::http::StatusCode::ok;
        }
    }

    if (version == qnStaticCommon->engineVersion())
    {
        result.setError(QnJsonRestResult::NoError, lit("UP_TO_DATE"));
        return nx::network::http::StatusCode::ok;
    }

    if (sysInfo != QnAppInfo::currentSystemInformation())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE_SYSTEM"));
        return nx::network::http::StatusCode::ok;
    }

    if (!m_updateTool->addUpdateFile(version.toString(), body))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("EXTRACTION_ERROR"));
        return nx::network::http::StatusCode::internalServerError;
    }

    if (!m_updateTool->installUpdate(version.toString()))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INSTALLATION_ERROR"));
        return nx::network::http::StatusCode::ok;
    }

    return nx::network::http::StatusCode::ok;
}

int QnUpdateRestHandler::handlePartialUpdate(const QString &updateId, const QByteArray &data, qint64 offset, QnJsonRestResult &result)
{
    qint64 chunkResult = m_updateTool->addUpdateFileChunkSync(updateId, data, offset);

    QnUploadUpdateReply reply;

    switch (chunkResult) {
    case QnServerUpdateTool::UploadFinished:
        reply.offset = ec2::AbstractUpdatesManager::NoError;
        break;
    case QnServerUpdateTool::NoFreeSpace:
        reply.offset = ec2::AbstractUpdatesManager::NoFreeSpace;
        break;
    case QnServerUpdateTool::UnknownError:
        reply.offset = ec2::AbstractUpdatesManager::UnknownError;
        break;
    case QnServerUpdateTool::NoReply:
        break;
    default:
        reply.offset = chunkResult;
        break;
    }

    result.setReply(reply);
    result.setError(QnJsonRestResult::NoError);
    return nx::network::http::StatusCode::ok;
}
