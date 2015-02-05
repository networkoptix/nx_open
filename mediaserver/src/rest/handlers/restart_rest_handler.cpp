#include "restart_rest_handler.h"

#include <common/common_module.h>
#include <media_server/settings.h>
#include "utils/network/tcp_connection_priv.h"

void restartServer(int restartTimeout);

int QnRestartRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor *) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    // TODO: #dklychkov refactor this to use a timeout.
    // We can't just write timeout here because this thread doesn't have an event loop.
    restartServer(0);

    result.setError(QnJsonRestResult::NoError);

    return CODE_OK;
}
