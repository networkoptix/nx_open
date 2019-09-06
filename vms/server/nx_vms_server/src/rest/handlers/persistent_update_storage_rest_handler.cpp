#include "persistent_update_storage_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/vms/server/update/update_manager.h>
#include <api/global_settings.h>
#include <nx/vms/server/settings.h>
#include <api/helpers/empty_request_data.h>
#include <rest/helpers/request_context.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/update/persistent_update_storage.h>

QnPersistentUpdateStorageRestHandler::QnPersistentUpdateStorageRestHandler(
    QnMediaServerModule* serverModule)
    :
    nx::vms::server::ServerModuleAware(serverModule)
{
}

namespace {

static const QString kVersionParam("version");

static bool checkVersionParam(const QnRequestParamList& params)
{
    return params.contains(kVersionParam)
        && (params[kVersionParam] == nx::update::kTargetKey
            || params[kVersionParam] == nx::update::kInstalledKey);
}

static int logAndMakeError(
    const QnEmptyRequestData& request,
    const QString& errorMessage,
    QByteArray* outBody,
    QByteArray* outContentType,
    nx::network::http::StatusCode::Value code = nx::network::http::StatusCode::badRequest)
{
    NX_WARNING(typeid(QnPersistentUpdateStorageRestHandler), errorMessage);
    return QnFusionRestHandler::makeError(
        code, errorMessage, outBody, outContentType, Qn::JsonFormat,
        request.extraFormatting, QnRestResult::CantProcessRequest);
}

} // namespace

int QnPersistentUpdateStorageRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(), params);

    if (!checkVersionParam(params))
        return logAndMakeError(request, "Invalid 'version' parameter", &result, &resultContentType);

    QList<QnUuid> serverList;
    if (!QJson::deserialize(body, &serverList))
    {
        return logAndMakeError(
            request, QString("Failed to deserialize request: (%1)").arg(QString::fromUtf8(body)),
            &result, &resultContentType);
    }

    serverModule()->updateManager()->setUpdatePersistentStorageServers(
        serverList, params[kVersionParam], /*manuallySet*/ !serverList.isEmpty());

    return QnFusionRestHandler::makeReply(
        serverModule()->updateManager()->updatePersistentStorageServers(params[kVersionParam]),
        params, &result, &resultContentType);
}

int QnPersistentUpdateStorageRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(), params);

    if (!checkVersionParam(params))
        return logAndMakeError(request, "Invalid 'version' parameter", &result, &contentType);

    return QnFusionRestHandler::makeReply(
        serverModule()->updateManager()->updatePersistentStorageServers(params[kVersionParam]),
        params, &result, &contentType);
}
