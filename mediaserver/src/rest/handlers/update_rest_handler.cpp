#include "update_rest_handler.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>

#include <utils/network/tcp_connection_priv.h>
#include <media_server/server_update_tool.h>
#include <utils/update/update_utils.h>
#include <utils/common/log.h>
#include <common/common_module.h>

int QnUpdateRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(result)
    Q_UNUSED(contentType)

    return CODE_NOT_IMPLEMETED;
}

int QnUpdateRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, const QByteArray& srcBodyContentType, QByteArray &result, QByteArray &contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(result)
    Q_UNUSED(contentType)
    Q_UNUSED(params)

    QnSoftwareVersion version;
    QnSystemInformation sysInfo;

    {
        QBuffer buffer(const_cast<QByteArray*>(&body)); // we're going to read data, so const_cast is safe
        if (!verifyUpdatePackage(&buffer, &version, &sysInfo)) {
            cl_log.log("An upload has been received but not veryfied", cl_logWARNING);
            return CODE_INVALID_PARAMETER;
        }
    }

    if (version == qnCommon->engineVersion())
        return CODE_OK;

    if (sysInfo != QnSystemInformation::currentSystemInformation())
        return CODE_INVALID_PARAMETER;

    if (!QnServerUpdateTool::instance()->addUpdateFile(version.toString(), body))
        return CODE_INTERNAL_ERROR;

    if (!QnServerUpdateTool::instance()->installUpdate(version.toString()))
        return CODE_INTERNAL_ERROR;

    return CODE_OK;
}
