#include "multiserver_update_request_helpers.h"
#include <nx/update/update_information.h>
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <rest/server/json_rest_result.h>

namespace detail {

void checkUpdateStatusRemotely(
    const IfParticipantPredicate& ifParticipantPredicate,
    QnMediaServerModule* serverModule,
    const QString& path,
    QList<nx::update::Status>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    static const QString kOfflineMessage = "peer is offline";
    auto mergeFunction =
        [serverModule, reply](
            const QnUuid& serverId, bool success, QnJsonRestResult& result, QnJsonRestResult&)
        {
            if (success)
            {
                const auto remotePeerUpdateStatus =
                    result.deserialized<QList<nx::update::Status>>()[0];

                if (remotePeerUpdateStatus.serverId.isNull())
                {
                    reply->append(nx::update::Status(
                        serverId, nx::update::Status::Code::offline, kOfflineMessage));
                }
                else
                {
                    reply->append(remotePeerUpdateStatus);
                }
            }
            else
            {
                if (serverId != serverModule->commonModule()->moduleGUID())
                {
                    reply->append(nx::update::Status(
                        serverId, nx::update::Status::Code::offline, kOfflineMessage));
                }
                else
                {
                    reply->append(serverModule->updateManager()->status());
                }
            }
        };

    QnJsonRestResult result;
    requestRemotePeers(
        serverModule->commonModule(), path, result, context, mergeFunction,
        ifParticipantPredicate);
    auto offlineServers = QSet<QnMediaServerResourcePtr>::fromList(
        serverModule->commonModule()->resourcePool()->getAllServers(Qn::Offline));

    for (const auto offlineServer: offlineServers)
    {
        if (ifParticipantPredicate)
        {
            const auto participationStatus = ifParticipantPredicate(
                offlineServer->getId(),
                offlineServer->getModuleInformation().version);

            if (participationStatus != ParticipationStatus::participant)
                continue;
        }

        if (std::find_if(reply->cbegin(), reply->cend(),
            [&offlineServer](const nx::update::Status& updateStatus)
            {
                return updateStatus.serverId == offlineServer->getId();
            }) != reply->cend())
        {
            continue;
        }

        reply->append(nx::update::Status(offlineServer->getId(), nx::update::Status::Code::offline,
            kOfflineMessage));
    }
}

IfParticipantPredicate makeIfParticipantPredicate(nx::CommonUpdateManager* updateManager)
{
    QnUuidList participants;
    if (!updateManager->participants(&participants))
        return nullptr;

    const auto targetVersion = updateManager->targetVersion();
    if (targetVersion.isNull())
        return nullptr;

    return
        [participants = QSet<QnUuid>::fromList(participants), targetVersion](
            const QnUuid& id,
            const nx::vms::api::SoftwareVersion& version)
        {
            if (!participants.isEmpty() && !participants.contains(id))
                return ParticipationStatus::notInList;

            return version <= targetVersion
                ? ParticipationStatus::participant : ParticipationStatus::incompatibleVersion;
        };
}

} // namespace detail
