#include "start_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/vms/server/update/update_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <api/global_settings.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx/vms/server/settings.h>
#include <nx/update/update_storages_helper.h>
#include "private/multiserver_update_request_helpers.h"
#include <rest/server/rest_connection_processor.h>

QnStartUpdateRestHandler::QnStartUpdateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnStartUpdateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& body,
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

    if (!serverModule()->updateManager()->updatePersistentStorageServers(
        nx::update::kTargetKey).manuallySet)
    {
        const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
            processor->resourcePool(), params);

        QnMultiserverRequestContext<QnEmptyRequestData> context(request,
            processor->owner()->getPort());

        nx::update::storage::ServerToStoragesList serversToStorages;
        detail::getStoragesDataRemotely(
            detail::makeIfParticipantPredicate(serverModule()->updateManager()),
            serverModule(), &serversToStorages, &context);

        const auto serverIds = nx::update::storage::selectServers(serversToStorages);
        serverModule()->updateManager()->setUpdatePersistentStorageServers(
            serverIds, nx::update::kTargetKey, /*manuallySet*/ false);
    }

    serverModule()->updateManager()->startUpdate(body);
    return nx::network::http::StatusCode::ok;
}
