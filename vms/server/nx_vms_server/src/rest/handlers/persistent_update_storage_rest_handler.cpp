#include "persistent_update_storage_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/vms/server/server_update_manager.h>
#include <api/global_settings.h>
#include <nx/vms/server/settings.h>
#include <api/helpers/empty_request_data.h>
#include <rest/helpers/request_context.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <rest/server/rest_connection_processor.h>

QnPersistentUpdateStorageRestHandler::QnPersistentUpdateStorageRestHandler(
    QnMediaServerModule* serverModule)
    :
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnPersistentUpdateStorageRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(), params);

    using namespace nx::network;
    const auto logAndMakeError =
        [&](const QString& errorMessage, http::StatusCode::Value code = http::StatusCode::badRequest)
        {
            NX_WARNING(this, errorMessage);
            return QnFusionRestHandler::makeError(
                code, errorMessage, &result, &resultContentType, Qn::JsonFormat,
                request.extraFormatting, QnRestResult::CantProcessRequest);
        };

    const QString kVersionParam("version");
    if (!params.contains(kVersionParam)
        || (params[kVersionParam] != "target" && params[kVersionParam] != "installed"))
    {
        return logAndMakeError("Invalid 'version' parameter");
    }

    QList<QnUuid> serverList;
    if (!QJson::deserialize(body, &serverList))
    {
        return logAndMakeError(
            QString("Failed to deserialize request: (%1)").arg(QString::fromUtf8(body)));
    }

    if (!serverModule()->updateManager()->updatePersistentStorageServers(
        serverList, params[kVersionParam]))
    {
        return logAndMakeError(
            QString("Failed to set new persistent storages list: (%1)").arg(QString::fromUtf8(body)),
            http::StatusCode::internalServerError);
    }


    return nx::network::http::StatusCode::ok;
}
