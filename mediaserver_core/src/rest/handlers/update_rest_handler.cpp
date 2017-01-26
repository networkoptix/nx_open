#include "update_rest_handler.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>

#include <managers/updates_manager.h>
#include <network/tcp_connection_priv.h>
#include <media_server/server_update_tool.h>
#include <utils/update/update_utils.h>
#include <nx/utils/log/log.h>
#include <common/common_module.h>
#include <api/model/upload_update_reply.h>
#include "core/resource_access/resource_access_manager.h"
#include "rest/server/rest_connection_processor.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"

int QnUpdateRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *processor)
{
    return executePost(path, params, QByteArray(), result, processor);
}

int QnUpdateRestHandler::executePost(
    const QString &path,
    const QnRequestParams &params,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* processor)
{
    Q_UNUSED(path)

    bool remotePeerHasAdminRights = qnResourceAccessManager->hasGlobalPermission(
        processor->accessRights(),
        Qn::GlobalAdminPermission);

    if (!remotePeerHasAdminRights)
    {
        QnUploadUpdateReply reply;
        reply.offset = ec2::AbstractUpdatesManager::UnknownError;
        result.setError(QnJsonRestResult::Error::Forbidden);
        result.setReply(reply);
        return nx_http::StatusCode::forbidden;
    }


    qint64 offset = params.value(lit("offset")).toLongLong();
    QString updateId = params.value(lit("updateId"));
    bool delayed = params.value(lit("delayed"), lit("false")) != lit("false");

    if (!updateId.isEmpty()) {
        if (params.contains(lit("offset"))) {
            return handlePartialUpdate(updateId, body, offset, result);
        } else if (body.isEmpty()) {
            QnUploadUpdateReply reply;
            bool res = true;

            if (delayed)
                QnServerUpdateTool::instance()->installUpdateDelayed(updateId);
            else
                res = QnServerUpdateTool::instance()->installUpdate(updateId);

            reply.offset = res ? ec2::AbstractUpdatesManager::NoError : ec2::AbstractUpdatesManager::UnknownError;
            result.setReply(reply);
            return nx_http::StatusCode::ok;
        }
    }

    QnSoftwareVersion version;
    QnSystemInformation sysInfo;

    {
        QBuffer buffer(const_cast<QByteArray*>(&body)); // we're going to read data, so const_cast is safe
        if (!verifyUpdatePackage(&buffer, &version, &sysInfo)) {
            cl_log.log("An upload has been received but not veryfied", cl_logWARNING);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INVALID_FILE"));
            return nx_http::StatusCode::ok;
        }
    }

    if (version == qnCommon->engineVersion()) {
        result.setError(QnJsonRestResult::NoError, lit("UP_TO_DATE"));
        return nx_http::StatusCode::ok;
    }

    if (sysInfo != QnSystemInformation::currentSystemInformation()) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE_SYSTEM"));
        return nx_http::StatusCode::ok;
    }

    if (!QnServerUpdateTool::instance()->addUpdateFile(version.toString(), body)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("EXTRACTION_ERROR"));
        return nx_http::StatusCode::internalServerError;
    }

    if (!QnServerUpdateTool::instance()->installUpdate(version.toString())) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INSTALLATION_ERROR"));
        return nx_http::StatusCode::ok;
    }

    return nx_http::StatusCode::ok;
}

int QnUpdateRestHandler::handlePartialUpdate(const QString &updateId, const QByteArray &data, qint64 offset, QnJsonRestResult &result) {
    qint64 chunkResult = QnServerUpdateTool::instance()->addUpdateFileChunkSync(updateId, data, offset);

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
    return nx_http::StatusCode::ok;
}
