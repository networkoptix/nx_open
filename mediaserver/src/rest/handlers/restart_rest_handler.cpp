#include "restart_rest_handler.h"

#include <common/common_module.h>
#include <media_server/settings.h>

void stopServer(int signal);

namespace HttpStatusCode {
    enum Code {
        ok = 200
    };
}

int QnRestartRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    Q_UNUSED(result)
    Q_UNUSED(params)

    stopServer(0);
    return HttpStatusCode::ok;
}

int QnRestartRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, const QByteArray &srcBodyContentType, QByteArray &result, QByteArray &resultContentType) {
    Q_UNUSED(body)
    Q_UNUSED(srcBodyContentType)

    return executeGet(path, params, result, resultContentType);
}
