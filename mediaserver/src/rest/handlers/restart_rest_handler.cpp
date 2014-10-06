#include "restart_rest_handler.h"

#include <common/common_module.h>
#include <media_server/settings.h>

void restartServer();

namespace HttpStatusCode {
    enum Code {
        ok = 200
    };
}

int QnRestartRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    Q_UNUSED(result)
    Q_UNUSED(params)

    restartServer();
    return HttpStatusCode::ok;
}

int QnRestartRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, const QByteArray &srcBodyContentType, QByteArray &result, 
                                      QByteArray &resultContentType, const QnRestConnectionProcessor* owner) 
{
    Q_UNUSED(body)
    Q_UNUSED(srcBodyContentType)

    return executeGet(path, params, result, resultContentType, owner);
}
