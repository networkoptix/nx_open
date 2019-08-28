#include "cancel_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/vms/server/update/update_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_access/resource_access_manager.h>

QnCancelUpdateRestHandler::QnCancelUpdateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnCancelUpdateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*resultContentType*/,
    const QnRestConnectionProcessor* processor)
{
    if (!serverModule()->resourceAccessManager()->hasGlobalPermission(
            processor->accessRights(), GlobalPermission::admin))
    {
        return nx::network::http::StatusCode::forbidden;
    }

    serverModule()->updateManager()->cancel();
    return nx::network::http::StatusCode::ok;
}
