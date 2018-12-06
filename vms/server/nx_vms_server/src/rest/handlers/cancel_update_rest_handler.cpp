#include "cancel_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/update/common_update_manager.h>

QnCancelUpdateRestHandler::QnCancelUpdateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnCancelUpdateRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    serverModule()->updateManager()->cancel();
    return nx::network::http::StatusCode::ok;
}
