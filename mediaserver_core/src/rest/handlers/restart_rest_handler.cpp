#include "restart_rest_handler.h"

#include <common/common_module.h>
#include <media_server/settings.h>
#include "network/tcp_connection_priv.h"

void restartServer(int restartTimeout);

int QnRestartRestHandler::executeGet(const QString& /*path*/, const QnRequestParams& /*params*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor*)
{
    result.setError(QnJsonRestResult::NoError);
    return CODE_OK;
}

void QnRestartRestHandler::afterExecute(
    const QString& /*path*/, const QnRequestParamList& /*params*/, const QByteArray& /*body*/,
    const QnRestConnectionProcessor*)
{
    restartServer(0);
}
