#include "update_rest_handler.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>

#include <utils/network/tcp_connection_priv.h>
#include <media_server/server_update_tool.h>
#include <utils/update/update_utils.h>
#include <utils/common/log.h>
#include <common/common_module.h>

int QnUpdateRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(result)

    return CODE_NOT_IMPLEMETED;
}

int QnUpdateRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(result)
    Q_UNUSED(params)

    QnSoftwareVersion version;
    QnSystemInformation sysInfo;

    {
        QBuffer buffer(const_cast<QByteArray*>(&body)); // we're going to read data, so const_cast is safe
        if (!verifyUpdatePackage(&buffer, &version, &sysInfo)) {
            cl_log.log("An upload has been received but not veryfied", cl_logWARNING);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INVALID_FILE"));
            return CODE_OK;
        }
    }

    if (version == qnCommon->engineVersion()) {
        result.setError(QnJsonRestResult::NoError, lit("UP_TO_DATE"));
        return CODE_OK;
    }

    if (sysInfo != QnSystemInformation::currentSystemInformation()) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE_SYSTEM"));
        return CODE_OK;
    }

    if (!QnServerUpdateTool::instance()->addUpdateFile(version.toString(), body)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("EXTRACTION_ERROR"));
        return CODE_INTERNAL_ERROR;
    }

    if (!QnServerUpdateTool::instance()->installUpdate(version.toString())) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INSTALLATION_ERROR"));
        return CODE_OK;
    }

    return CODE_OK;
}
