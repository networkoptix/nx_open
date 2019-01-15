#include "finish_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/vms/server/server_update_manager.h>
#include "private/multiserver_request_helper.h"
#include <rest/server/rest_connection_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>

QnFinishUpdateRestHandler::QnFinishUpdateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

bool QnFinishUpdateRestHandler::allPeersUpdatedSuccessfully() const
{
    auto onlineServers = QSet<QnMediaServerResourcePtr>::fromList(
        serverModule()->commonModule()->resourcePool()->getAllServers(Qn::Online));
    const auto offlineServers = QSet<QnMediaServerResourcePtr>::fromList(
        serverModule()->commonModule()->resourcePool()->getAllServers(Qn::Offline));

    QnUuidList participantsIds;
    if (!serverModule()->updateManager()->participants(&participantsIds))
        return false;

    const bool anyParticipantOffline = std::any_of(offlineServers.cbegin(), offlineServers.cend(),
        [&participantsIds](const auto& server)
        {
            return participantsIds.contains(server->getId());
        });

    if (anyParticipantOffline)
        return false;

    const auto participants = detail::filterOutNonParticipants(onlineServers, participantsIds);
    const auto targetVersion = serverModule()->updateManager()->targetVersion();
    if (targetVersion.isNull())
        return false;

    return std::all_of(participants.cbegin(), participants.cend(),
        [&targetVersion](const auto& server)
        {
            return server->getModuleInformation().version >= targetVersion;
        });
}

int QnFinishUpdateRestHandler::executePost(const QString& /*path*/,
    const QnRequestParamList& params, const QByteArray& body,
    const QByteArray& /*srcBodyContentType*/, QByteArray& result,
    QByteArray& resultContentType, const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(), params);

    if (params.contains("ignorePendingPeers") || allPeersUpdatedSuccessfully())
    {
        serverModule()->updateManager()->finish();
        return nx::network::http::StatusCode::ok;
    }

    return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
        "Not all peers have been successfully updated", &result, &resultContentType,
        Qn::JsonFormat, request.extraFormatting, QnRestResult::CantProcessRequest);
}
