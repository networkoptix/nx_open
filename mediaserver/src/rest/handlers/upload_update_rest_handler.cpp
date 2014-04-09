#include "upload_update_rest_handler.h"

#include <media_server/server_update_tool.h>
#include <utils/network/tcp_connection_priv.h>

int QnUploadUpdateRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(params)
    Q_UNUSED(path)
    Q_UNUSED(contentType)

    result.append("<root>Invalid method 'GET'. You should use 'POST' instead.</root>\n");

    return CODE_NOT_IMPLEMETED;
}

int QnUploadUpdateRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    QString updateId;
    for (int i = 0; i < params.size(); i++) {
        if (params[i].first == "update_id")
            updateId = params[i].second;
    }

    if (updateId.isEmpty())
        return CODE_INVALID_PARAMETER;

    if (QnServerUpdateTool::instance()->addUpdateFile(updateId, body)) {
        return CODE_OK;
    } else {
        return CODE_INTERNAL_ERROR;
    }
}

QString QnUploadUpdateRestHandler::description() const {
    return QLatin1String(
        "Upload an update file to the server\n"
        "<br>Param <b>update_id</b> - string update identifier\n"
        "<br><b>POST body</b> - contents of update file\n"
    );
}
