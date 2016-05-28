#include "restart_rest_handler.h"

#include <common/common_module.h>
#include <media_server/settings.h>
#include "network/tcp_connection_priv.h"

void restartServer(int restartTimeout);

int QnRestartRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *) {
    Q_UNUSED(path)
    Q_UNUSED(params)
    result.setError(QnJsonRestResult::NoError);
    return CODE_OK;
}

void QnRestartRestHandler::afterExecute(const nx_http::Request& request, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor*)
{
    Q_UNUSED(request)
    Q_UNUSED(params)
    Q_UNUSED(body)

    restartServer(0);
}
