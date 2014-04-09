#include "update_rest_handler.h"

#include <media_server/server_update_tool.h>
#include <utils/network/tcp_connection_priv.h>

int QnUpdateRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(contentType)

    QString updateId;
    for (int i = 0; i < params.size(); i++) {
        if (params[i].first == "updateId")
            updateId = params[i].second;
    }

    if (updateId.isEmpty())
        return CODE_INVALID_PARAMETER;

    if (QnServerUpdateTool::instance()->installUpdate(updateId)) {
        return CODE_OK;
    } else {
        return CODE_INTERNAL_ERROR;
    }
}

int QnUpdateRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(body)

    return executeGet(path, params, result, contentType);
}

QString QnUpdateRestHandler::description() const {
    return QLatin1String(
        "Install previously uploaded update\n"
        "<br>Param <b>update_id</b> - string update identifier\n"
    );
}
