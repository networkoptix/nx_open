#include "start_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/vms/server/update/update_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <api/global_settings.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx/vms/server/settings.h>

QnStartUpdateRestHandler::QnStartUpdateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnStartUpdateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& body,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*resultContentType*/,
    const QnRestConnectionProcessor* processor)
{
    serverModule()->updateManager()->startUpdate(body);
    return nx::network::http::StatusCode::ok;
}
