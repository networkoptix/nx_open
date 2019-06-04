#include "start_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/vms/server/server_update_manager.h>
#include <api/global_settings.h>
#include <nx/vms/server/settings.h>
#include <nx/update/update_storages_helper.h>

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
    const QnRestConnectionProcessor* /*processor*/)
{

    serverModule()->updateManager()->startUpdate(body);
    return nx::network::http::StatusCode::ok;
}
