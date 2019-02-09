#include "finish_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/vms/server/server_update_manager.h>
#include "private/multiserver_update_request_helpers.h"
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
    using namespace detail;
    auto servers = QSet<QnMediaServerResourcePtr>::fromList(
        serverModule()->commonModule()->resourcePool()->getAllServers(Qn::AnyStatus));
    const auto ifParticipantPredicate =
        makeIfParticipantPredicate(serverModule()->updateManager());

    return ifParticipantPredicate && std::all_of(servers.cbegin(), servers.cend(),
        [&ifParticipantPredicate,
        targetVersion = serverModule()->updateManager()->targetVersion()](const auto& server)
        {
            const auto serverVersion = server->getModuleInformation().version;
            switch (ifParticipantPredicate(server->getId(), serverVersion))
            {
                case ParticipationStatus::participant:
                    return serverVersion == targetVersion;
                case ParticipationStatus::notInList:
                case ParticipationStatus::incompatibleVersion:
                    return true;
            }
            return false;
        });
}

int QnFinishUpdateRestHandler::executePost(
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

    if (params.contains("ignorePendingPeers") || allPeersUpdatedSuccessfully())
    {
        serverModule()->updateManager()->finish();
        return nx::network::http::StatusCode::ok;
    }

    return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
        "Not all peers have been successfully updated", &result, &resultContentType,
        Qn::JsonFormat, request.extraFormatting, QnRestResult::CantProcessRequest);
}
